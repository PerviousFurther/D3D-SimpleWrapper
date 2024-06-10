#pragma once

namespace twen 
{
	struct Fence final
	{
		Fence(Device& device, ::D3D12_FENCE_FLAGS flags = ::D3D12_FENCE_FLAG_NONE)
		{
			device.Verify(
				device->CreateFence(0u, flags, IID_PPV_ARGS(&m_Fence))
			);
			MODEL_ASSERT(m_Fence, "Create fence failure.");
		}

		Fence(Fence&&) = default;
		Fence& operator=(Fence&&) = default;

		~Fence() 
		{
			m_Fence->Release();
		}
	private:
		::ID3D12Fence* m_Fence;
	};

	struct SyncPoint 
	{
		::UINT64 FenceValue;

		Fence* Fence;
	};

}

namespace twen
{
	class CommandContext
		: inner::DeviceChild
	{
		friend class Device;
	public:
		using command_list_type = ::ID3D12GraphicsCommandList4;
		using command_list_allocator = ::ID3D12CommandAllocator;

		class DirectCommandContext;
		class ComputeCommandContext;
		class CopyCommandContext;

		// Types that the command context support currently.
		TWEN_ISC::D3D12_COMMAND_LIST_TYPE Types[]
		{
			::D3D12_COMMAND_LIST_TYPE_DIRECT,
			::D3D12_COMMAND_LIST_TYPE_COPY,
			::D3D12_COMMAND_LIST_TYPE_COMPUTE,
		};
	private:

		using payload_type = ::ID3D12CommandList*;
		using states_type = ::std::vector<::D3D12_RESOURCE_STATES>;

		struct ResourceStateTracker;

		template<typename Child>
		struct BindableContext;

		template<typename Child>
		struct BarrierContext;

		template<typename Child>
		struct CopyContext;

		struct ContextCommon;

	public:

		CommandContext(CommandContext const&) = delete;
		CommandContext& operator=(CommandContext const&) = delete;
		~CommandContext();

	public:

		// We assume the giving function is can be invoked with command context.
		template<typename Fn>
		void ExecuteOnMain(/*Device* device, */Fn&& fn)
			requires::std::invocable<Fn, CommandContext::DirectCommandContext*>;

		// We assume the giving function is can be invoked with copy context.
		template<typename Fn>
		void ExecuteOnBatch(/*Device* device, */Fn&& fn) requires
			::std::invocable<Fn, CommandContext::CopyCommandContext*> ||
			::std::invocable<Fn, CommandContext::ComputeCommandContext*> ||
			::std::invocable<Fn, CommandContext::DirectCommandContext*>;

		void Commit();

	private:
		CommandContext(Device& device) 
			: DeviceChild{device}
		{
			// TODO: ?
			for (auto const& type : Types) 
			{
				device.Verify(
					device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_Allocators[type]))
				);
			}
		}
	private:
		template<typename T>
		T* CreateNewContext(Device& device)
		{
			if (!GetDevice().m_ResidencyManager->Recording())
				GetDevice().m_ResidencyManager->BeginRecordingResidentChanging();

			command_list_type* cmdlist{nullptr};
			device.Verify(
				device->CreateCommandList(device.AllVisibleNode(), T::Type, m_Allocators.at(T::Type), nullptr, IID_PPV_ARGS(&cmdlist))
			);

			MODEL_ASSERT(cmdlist, "Create command list failure.");
			
			return new T{ this, cmdlist, T::Type };
		}

		void Close(ContextCommon* common);

	private:
		::std::unordered_map<::D3D12_COMMAND_LIST_TYPE,
			command_list_allocator*> m_Allocators;

		::std::vector<ContextCommon*> m_ActiveContexts{};

		::std::unordered_map<::D3D12_COMMAND_LIST_TYPE, 
			::std::vector<payload_type>> m_Payloads;

		::std::unordered_map<Resource*, 
			ResourceStateTracker*> m_States{};
	};

												//===============================/\
												
												//              *                =
																			      
												//       Resource barrier        =
																			      
												//              *                =

												//===============================/\

	struct CommandContext::ResourceStateTracker 
	{
		using state_t = ::D3D12_RESOURCE_STATES;
		// TODO: Currently, we dont know when should the state update. 
		//       If other thread also need to transition the state, what barrier should it access.
		bool UpdateState(::D3D12_RESOURCE_STATES state, ::UINT subresourceIndex, ::std::vector<::D3D12_RESOURCE_BARRIER>& barriers)
		{
			::std::unique_lock _{ Mutex };

			MODEL_ASSERT(subresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ? true : subresourceIndex < States.size(), "Out of range.");

			if (subresourceIndex != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
			{
				auto oldState = States.at(subresourceIndex);

				// We currently only check the state is same with before.
				if (oldState == state)
					return false;

				static_cast<void>(state == IdleState ? NumNotIdle-- : NumNotIdle++);

				States.at(subresourceIndex) = state;

				barriers.emplace_back(Utils::ResourceBarrier(*Resource, oldState, state, subresourceIndex));
			} else {
				if (state == IdleState) NumNotIdle = 0u;
				else NumNotIdle = static_cast<::UINT>(States.size());

				// TODO: find better way.
				for (auto index{ 0u }; auto& oldState : States) 
				// Only emplace the state that is not same.
				{
					if (oldState != state)
					{
						barriers.emplace_back(Utils::ResourceBarrier(*Resource, oldState, state, index));
						oldState = state;
					}

					index++;
				}
			}
			return true;
		}

		template<typename View, typename Ext>
		ResourceStateTracker(::std::shared_ptr<Views::Bindable<View, Ext>> bindable) 
			: Resource{ bindable->BackingResource() }
			, IdleState{ inner::Transition<View>{}(ResourceCurrentStage::Idle) }
		{
			if (bindable->IsTexture())
				States.resize(bindable->ResourcePosition().NumSubresource, IdleState);
			else
				States.resize(1u, IdleState);
		}

		state_t IdleState  {};
		::UINT  NumNotIdle {0u}; 
		Resource* Resource {nullptr};
		::std::mutex Mutex {};
		::std::vector<state_t> States{};
	};
	//
	//
	//
	//
	template<typename Child>
	struct CommandContext::BarrierContext
	{
		template<typename View, typename Ext>
		using shared_view = ::std::shared_ptr<Views::Bindable<View, Ext>>;

		// Transition the view to specified stage. this method was recommanded.
		template<typename View, typename Ext>
		bool Transition(shared_view<View, Ext> view, ResourceCurrentStage nextStage) noexcept
		{
			auto state = inner::Transition<View>{}(nextStage);

			return Transition(view, state);
		}

		// Transition the view to specified state.
		template<typename View, typename Ext>
		bool Transition(shared_view<View, Ext> view, ::D3D12_RESOURCE_STATES newState) noexcept
		{
			CommandContext* context = static_cast<const Child*>(this)->ParentContext();
			auto resource = view->BackingResource();

			// TODO: do check.

			if (!context->m_States.contains(resource))
				context->m_States.emplace(resource, new ResourceStateTracker{view});

			return context->m_States.at(resource)->UpdateState(newState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, m_Barriers);
		}
		template<typename View, typename Ext>
		bool Transition(shared_view<View, Ext> view, ::D3D12_RESOURCE_STATES before, ::D3D12_RESOURCE_STATES newState) noexcept
		{
			CommandContext* context = static_cast<const Child*>(this)->ParentContext();

			// TODO: do check.
			auto resource{ view->BackingResource() };

			if (!context->m_States.contains(resource))
				context->m_States.emplace(resource, new ResourceStateTracker{ view });

			return context->m_States.at(resource)->UpdateState(newState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, m_Barriers);
		}
		// Clean up all storaged barrier.
		void FlushBarriers() 
		{ 
			MODEL_ASSERT(m_Barriers.size(), "No need to call FlushBarrier().");
			static_cast<Child*>(this)->m_CommandList->ResourceBarrier(static_cast<::UINT>(m_Barriers.size()), m_Barriers.data());
			m_Barriers.clear();
		}
		// TODO: alias...
		// TODO: uav barrier...

		::std::vector<::D3D12_RESOURCE_BARRIER> m_Barriers{};
	};


											//===============================/\
											
											//              *                = 

											//        Resource binding.      =

											//              *                =

											//===============================/\


	template<typename Child>
	struct CommandContext::BindableContext
	{
		template<typename View, typename Ext>
		using shared_view = ::std::shared_ptr<Views::Bindable<View, Ext>>;

		using root_signature = RootSignature*;
		using pipeline_state = Pipeline*;

		void BindDescriptorHeap() 
		{
			DescriptorManager* descriptorManager = static_cast<Child*>(this)
				->ParentContext()->GetDevice().DescriptorManager();
			auto heaps = descriptorManager->GetHeaps();
			static_cast<Child*>(this)->CommandList()->SetDescriptorHeaps(2u, heaps.data());
		}
		void InitDescriptor(::D3D12_DESCRIPTOR_HEAP_TYPE type, inner::Pointer<DescriptorHeap>& pointer, ::UINT num)
		{
			if (pointer.Vaild()) 
				pointer.Fallback();

			pointer.Size = num;
			pointer.Backing = nullptr;

			DescriptorManager* descriptorManager = static_cast<Child*>(this)
				->ParentContext()->GetDevice().DescriptorManager();
			// TODO: Very temporary...
			if (num) 
				pointer = descriptorManager->GetDescriptors(type, num);
		}

		~BindableContext() 
		{
			for (auto const& [_, ptr] : m_DescriptorHandles) 
				ptr.Pointer.Fallback();
		}

		struct Info
		{
			inner::Pointer<DescriptorHeap> Pointer;
			::UINT8 Bound{ 0u };
		};

		/*void BindDefualtDescriptorHeap() 
		{
			static_cast<Child*>(this)->GetDevice
		}*/

		// Bind resource guide by shader input binding desc.
		template<typename View, typename Ext>
		bool Bind(::std::shared_ptr<Views::Bindable<View, Ext>> view, Shaders::Input const& bind) requires
			// which means the view can be bind in any context. When the constrain is not satisfied.
			// it means the view cannot bind on current context.
			requires{ typename View::common_bind; }
		{
			MODEL_ASSERT(m_RootSignature, "Please set root signature at first.");

			Roots::ParameterType type;
			// get binding desc from shader.
			auto rootType{ Utils::ToRootParameterType(bind.Type) };
			// should on descriptor table.
			if (rootType)
				type = Roots::ParameterTypeFrom(Utils::ToRootParameterType(bind.Type));
			else
				type = Roots::ParameterTypeFrom(Utils::ToDescriptorRangeType(bind.Type));

			if (type == 0xff) return false;

			// Get descriptor info.
			auto const& descriptor = m_RootSignature->Map(bind.BindPoint, bind.Space, type);
			if (descriptor.OnTable()) 
			// descriptor on table.
			{
				auto& handles{m_DescriptorHandles}; // alias. nothing special.
				auto it = handles.find(static_cast<::UINT16>(descriptor.RootIndex));
				if (it != handles.end()) 
				// the allocation was recorded.
				{
					Info& info{it->second};
					// Create view.
					inner::Operation<View, Child::Type>{}(view, info.Pointer[info.Bound]);

					// if it is full then set descriptor table
					if (++info.Bound == info.Pointer.Size)
						SetDescriptorTable<Child::Type>(descriptor.RootIndex, info.Pointer);
				} else {
					if (m_DescriptorHandles.empty())
						BindDescriptorHeap();
				// table wasnt bound, then allocate it from manager.
					auto const& [iter, result] {handles.emplace(descriptor.RootIndex, Info{})};
					MODEL_ASSERT(result, "BUG: emplace failure.");

					Info& info{iter->second};
					// allocate.
					InitDescriptor(
						View::DescriptorHeapType, 
						info.Pointer,
						// Get num of descriptor in the table.
						m_RootSignature->NumAllocation(descriptor.RootIndex));
					// Create view.
					inner::Operation<View, Child::Type>{}(view, info.Pointer[info.Bound]);

					// if it is full then set descriptor table (some maybe only one.)
					if (++info.Bound == info.Pointer.Size)
						SetDescriptorTable<Child::Type>(descriptor.RootIndex, info.Pointer);
				}
			} else {
			// descriptor is root descriptor.
				if constexpr (requires{ inner::Operation<View, Child::Type>{}; })
					inner::Operation<View, Child::Type>{}(view, static_cast<Child*>(this)->CommandList(), descriptor.RootIndex);
				else MODEL_ASSERT(false, "BUG: Sampler cannot bind on root.");
			}

			return true;
		}

		// This is reserved for bindless render.
		// Setting descriptor heap for dynamic sampler and on heap resource descriptors.
		// @return false means descriptor heap was not bind.
		//bool BindDescriptor(inner::Pointer<DescriptorHeap> const& sampler, inner::Pointer<DescriptorHeap> const& csu) noexcept
		//{
		//	// descriptors pointer are too small.
		//	if (csu.Size < m_DescriptorsResource.Size || sampler.Size < m_DescriptorsSampler.Size)
		//		return false;
		//	::ID3D12DescriptorHeap* heaps[2]{};
		//	::UINT count{ 0u };
		//	if (sampler.Size)
		//		heaps[count++] = *sampler.Backing.lock();
		//	if (csu.Size)
		//		heaps[count++] = *csu.Backing.lock();
		//	if (count)
		//	{
		//		static_cast<Child*>(this)->CommandList()->SetDescriptorHeaps(count, heaps);
		//		return true;
		//	}
		//	else return false;
		//}

		template<::D3D12_COMMAND_LIST_TYPE type>
		void SetDescriptorTable(::UINT root, inner::Pointer<DescriptorHeap> const& pointer, ::UINT index = 0u) 
		{
			if constexpr (type == ::D3D12_COMMAND_LIST_TYPE_DIRECT)
				static_cast<Child*>(this)->CommandList()->SetGraphicsRootDescriptorTable(root, pointer(index));
			else if constexpr(type == ::D3D12_COMMAND_LIST_TYPE_COMPUTE)
				static_cast<Child*>(this)->CommandList()->SetComputeRootDescriptorTable(root, pointer(index));
		}
		// TODO: bindless...

		Pipeline* m_Pipeline{};
		RootSignature* m_RootSignature{};

		// [Root, pointer]
		// Allocation of descriptor heap.
		::std::unordered_map<::UINT16, Info> m_DescriptorHandles;
	};

										//===============================/\
										
										//              *                =

										// Resource copy and interaction =

										//              *                =

										//===============================/\

	template<typename Child>
	struct CommandContext::CopyContext 
	{
		// The method consume the dst view and src view have common characterist in them (subresource index/range).
		// The behavior of this method is:
		// 
		// - If only one of the resource is texture. then the copy operation will based on that resource. 
		//   Operation will interrupt if the buffer dont have enough space.
		// - If both of the resource are texture, then copy the view of them.
		//   Operation wont check though there have any potential risks.
		// - If both of the resource is buffer, then select smaller range to copy.
		template<typename DstView, typename SrcView>
		void Copy(::std::shared_ptr<DstView> dstView, ::std::shared_ptr<SrcView> srcView) 
		{
			using tex_copy = ::D3D12_TEXTURE_COPY_LOCATION;

			::twen::Resource* dst = dstView->BackingResource();
			::twen::Resource* src = srcView->BackingResource();
			
			const auto srcIsTexture{ src->Desc.Dimension != ::D3D12_RESOURCE_DIMENSION_BUFFER };
			const auto dstIsTexture{ dst->Desc.Dimension != ::D3D12_RESOURCE_DIMENSION_BUFFER };

			inner::Pointer<Resource> const& srcPosition{ srcView->ResourcePosition() };
			inner::Pointer<Resource> const& dstPosition{ dstView->ResourcePosition() };
			command_list_type* cmdList = static_cast<Child*>(this)->CommandList();

			if (dstIsTexture && srcIsTexture) // both texture.
			{
				auto size = (::std::min)(dstPosition.NumSubresource, srcPosition.NumSubresource);
				for (auto index{ 0u }; index < size; index++) 
				{
					auto srcCopy = Utils::TextureCopy(*src, srcPosition.SubresourceIndex + index),
					     dstCopy = Utils::TextureCopy(*dst, dstPosition.SubresourceIndex + index);

					cmdList->CopyTextureRegion(&dstCopy, 0u, 0u, 0u, &srcCopy, nullptr);
				}

			} else if (dstIsTexture || srcIsTexture) { // one of the view is texture.

				auto footprints{ dstIsTexture ? dst->Footprints() : src->Footprints() };
				auto size{ dstIsTexture ? dstPosition.NumSubresource : srcPosition.NumSubresource };

				for (auto index{0u}; index < size; index++)
				{
					tex_copy srcCopy;
					tex_copy dstCopy;

					if (dstIsTexture) 
					{
						auto footprint{ footprints[dstPosition.SubresourceIndex + index] };
						// Cannot continue copy operation.
						if ((footprint.Offset + static_cast<::UINT64>(footprint.Footprint.RowPitch) * footprint.Footprint.Height * footprint.Footprint.Depth) > srcPosition.Size)
							return;

						srcCopy = Utils::TextureCopy(*src, footprints[dstPosition.SubresourceIndex + index]),
						dstCopy = Utils::TextureCopy(*dst, dstPosition.SubresourceIndex + index);

					} else {

						auto footprint{ footprints[srcPosition.SubresourceIndex + index] };
						// Cannot continue copy operation.
						if ((footprint.Offset + static_cast<::UINT64>(footprint.Footprint.RowPitch) * footprint.Footprint.Height * footprint.Footprint.Depth) > dstPosition.Size)
							return;

						dstCopy = Utils::TextureCopy(*dst, footprints[srcPosition.SubresourceIndex + index]),
						srcCopy = Utils::TextureCopy(*src, srcPosition.SubresourceIndex + index);
					}

					cmdList->CopyTextureRegion(&dstCopy, 0u, 0u, 0u, &srcCopy, nullptr);
				}

			} else { // both buffer.
				auto size{ dstPosition.Size > srcPosition.Size ? srcPosition.Size : dstPosition.Size };

				cmdList->CopyBufferRegion(*dst, dstPosition.Offset, *src, srcPosition.Offset, size);
			}

		}

		// TODO: sub range copy.

		bool AssertOnState(auto&&...) 
		{
			return true;
		}
	};

												//===============================/\
												
												//              *                =

												//           Contexts            =

												//              *                =

												//===============================/\

	struct CommandContext::ContextCommon 
	{
		friend class CommandContext;

		CommandContext* ParentContext() const { return m_ParentContext; }
		command_list_type* CommandList() const { return m_CommandList; }
		
		ContextCommon(CommandContext* parentContext, command_list_type* cmdList, ::D3D12_COMMAND_LIST_TYPE type) 
			: m_ParentContext{parentContext} 
			, m_CommandList{cmdList} 
			, Type{type}
		{}

		~ContextCommon() = default;

		const::D3D12_COMMAND_LIST_TYPE Type;

	protected:

		CommandContext* const m_ParentContext;
		command_list_type* const m_CommandList;

		// Specified the command list open or not.
		bool IsOpen        : 1 {false};
		// TODO : ?
		bool OnQuery       : 1 {false};
		// TODO : ?
		bool IsPredicating : 1 {false}; 
		// Indicate current command list is submitted.
		bool Submmited     : 1 {false};
		// indicate current command list is finished or not.
		bool Finished      : 1 {false};
	};

	class CommandContext::DirectCommandContext 
		: public ContextCommon
		, CopyContext<DirectCommandContext>
		, BarrierContext<DirectCommandContext>
		, BindableContext<DirectCommandContext>
	{
		friend class CommandContext;
	public:
		// Indicate command list type of current context.
		TWEN_ISCA Type{ ::D3D12_COMMAND_LIST_TYPE_DIRECT };

		using ContextCommon::ContextCommon;
		using ContextCommon::CommandList;
		using ContextCommon::ParentContext;
		using CopyContext::Copy;
		using BarrierContext::Transition;
		using BindableContext::Bind;
		using BarrierContext::FlushBarriers;

		~DirectCommandContext() 
		{
			if (m_DescriptorsRenderTarget.Vaild()) 
				m_DescriptorsRenderTarget.Fallback();

			if (m_DescriptorsDepthStencil.Vaild())
				m_DescriptorsDepthStencil.Fallback();
		}
		// Bind render target or depth stencil to current context.
		// render target wont have extension currently.
		// @return false means pipeline was not set or no need to bind depth stencil.
		template<typename View>
		bool Bind(::std::shared_ptr<Views::Bindable<View>> renderTargetOrDepthStencil, ::UINT index = 0u) requires
			(Views::is_rtv<View> || Views::is_dsv<View>)
		{
			MODEL_ASSERT(renderTargetOrDepthStencil, "Do not try to set empty render target.");
			auto& pointer{ Views::is_rtv<View> ? m_DescriptorsRenderTarget : m_DescriptorsDepthStencil };

			if (!pointer.Size) return false;
			BindableContext::InitDescriptor(View::DescriptorHeapType, pointer, static_cast<::UINT>(pointer.Size));

			inner::Operation<View, Type>{}(renderTargetOrDepthStencil, pointer[index]);
			MODEL_ASSERT(!ParentContext()->m_States.contains(renderTargetOrDepthStencil->BackingResource()), "Should not bind this render target before.");

			BarrierContext::Transition(renderTargetOrDepthStencil, ResourceCurrentStage::Write);

			return true;
		}

		// Setting root signature.
		void RootSignature(root_signature rootSignature) 
		{
			BindableContext::m_RootSignature = rootSignature;
			m_CommandList->SetGraphicsRootSignature(*rootSignature);
		}
		// Setting legacy pipeline state.
		void Pipeline(GraphicsPipelineState* state) 
		{
			BindableContext::m_Pipeline = state;
			m_CommandList->SetPipelineState(*state);

			m_DescriptorsRenderTarget.Size = state->RenderTargetCount;
			m_DescriptorsDepthStencil.Size = state->DepthEnable ? state->RenderTargetCount : 0u;
		}

		// Clear render target and depth stencil (if pipeline enable depth writing.). Must set render target before this is call.
		void ClearTarget(bool renderTarget = false, bool depth = false, bool stencil = false, ::std::vector<::D3D12_RECT> const& rect = {})
		{
			auto rtvHandle{ m_DescriptorsRenderTarget[0] };
			auto dsvHandle{ m_DescriptorsDepthStencil[0] };
			m_CommandList->OMSetRenderTargets((::UINT)m_DescriptorsRenderTarget.Size, &rtvHandle, true, m_DescriptorsDepthStencil.Size ? &dsvHandle : nullptr);

			for (auto offset{ 0u }; offset < m_DescriptorsRenderTarget.Size; ++offset)
			{
				if (renderTarget) 
					m_CommandList->ClearRenderTargetView(
					m_DescriptorsRenderTarget[offset], 
					Constants::ClearValue, 
					static_cast<::UINT>(rect.size()), 
					rect.data());
				if (depth || stencil) 
					m_CommandList->ClearDepthStencilView(
					m_DescriptorsDepthStencil[offset], 
					static_cast<::D3D12_CLEAR_FLAGS>(::D3D12_CLEAR_FLAG_DEPTH * depth | ::D3D12_CLEAR_FLAG_STENCIL * stencil), 
					Constants::DepthClear, 
					Constants::StencilClear, 
					static_cast<::UINT>(rect.size()), 
					rect.data());
			}
		}

		// Not recommanded.
		// Set viewport by { 0, 0, width, height }.
		void Viewport(::UINT width, ::UINT height) 
		{
			::D3D12_VIEWPORT view
			{
				.Width{static_cast<float>(width)},
				.Height{static_cast<float>(height)},
				.MaxDepth{1.0f},
			};
			::D3D12_RECT rect
			{
				.right{static_cast<LONG>(width)},
				.bottom{static_cast<LONG>(height)},
			};
			m_CommandList->RSSetViewports(1u, &view);
			m_CommandList->RSSetScissorRects(1u, &rect);
		}

		// Implicit transition of resource state would take place on the render target and become idle state (...might be common).
		// So we recommand you call this method before commit.
		void Copy(::std::shared_ptr<RenderBuffer> renderTarget, ::std::shared_ptr<SwapchainBuffer> buffer) 
		{
			BarrierContext::Transition(renderTarget, ResourceCurrentStage::Copy);
			BarrierContext::m_Barriers.emplace_back(Utils::ResourceBarrier(*buffer, ::D3D12_RESOURCE_STATE_PRESENT, ::D3D12_RESOURCE_STATE_RESOLVE_DEST));
			FlushBarriers();

			m_CommandList->ResolveSubresource(*buffer, 0u, *renderTarget->BackingResource(), renderTarget->ResourcePosition().SubresourceIndex, ::DXGI_FORMAT_R8G8B8A8_UNORM);

			BarrierContext::Transition(renderTarget, ResourceCurrentStage::Idle);
			BarrierContext::m_Barriers.emplace_back(Utils::ResourceBarrier(*buffer, ::D3D12_RESOURCE_STATE_RESOLVE_DEST, ::D3D12_RESOURCE_STATE_PRESENT));
			FlushBarriers();
		}

		// Implicit transition of resource state would take place on the render target and become idle state (...might be common).
		// So we recommand you call the method on main thread before commit.
		// @return represent the operation is completed.
		bool Copy(::std::shared_ptr<RenderBuffer> renderTarget, IDXGISwapChain4* swapchain) noexcept(!D3D12_MODEL_DEBUG)
		{
			::ID3D12Resource* buffer{nullptr};
			if (FAILED(swapchain->GetBuffer(swapchain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&buffer)))) 
			{
				if (buffer) buffer->Release();
				return false;
			}
		#if D3D12_MODEL_DEBUG
			auto&& desc = buffer->GetDesc();
			// TODO: Would format be typeless?
			if (desc.Format != renderTarget->BackingResource()->Desc.Format)
				return false;
		#endif

			BarrierContext::Transition(renderTarget, ResourceCurrentStage::Copy);
			BarrierContext::m_Barriers.emplace_back(Utils::ResourceBarrier(buffer, ::D3D12_RESOURCE_STATE_PRESENT, ::D3D12_RESOURCE_STATE_RESOLVE_DEST));
			FlushBarriers();

			m_CommandList->ResolveSubresource(buffer, 0u, *renderTarget->BackingResource(), renderTarget->ResourcePosition().SubresourceIndex, ::DXGI_FORMAT_R8G8B8A8_UNORM);

			BarrierContext::Transition(renderTarget, ResourceCurrentStage::Idle);
			BarrierContext::m_Barriers.emplace_back(Utils::ResourceBarrier(buffer, ::D3D12_RESOURCE_STATE_RESOLVE_DEST, ::D3D12_RESOURCE_STATE_PRESENT));
			FlushBarriers();

			buffer->Release();

			return true;
		}
		// set vbo.
		void Bind(::std::shared_ptr<VertexBuffer> vbo, ::UINT countOfSlot = 1u, ::UINT startSlot = 0u) 
		{
			MODEL_ASSERT(vbo->CanBind(), "vbo didnt init.");
			MODEL_ASSERT(vbo->View().size(), "vbo havent be written.");

			MODEL_ASSERT(vbo->Address() == vbo->View().front().BufferLocation, "Not from start.");

			m_CommandList->IASetVertexBuffers(startSlot, countOfSlot, vbo->View().data() + startSlot);
		}
		// set ibo.
		void Bind(::std::shared_ptr<IndexBuffer> ibo) 
		{
			MODEL_ASSERT(ibo->CanBind(), "ibo didnt init.");
			m_CommandList->IASetIndexBuffer(&ibo->View());
		}

		// simply pack up.
		struct DrawParameter 
		{
			::UINT VertexCountPerInstance;
			::UINT InstanceCount              {1u};
			::UINT StartVertexLocation        {0u};
			::UINT StartInstanceLocation      {0u};
			::D3D_PRIMITIVE_TOPOLOGY Topology {::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST};
		};
		// Instance count defualt at 1, Topology default at TRIANGLELIST.
		//  |w|
		// tips: You can invoke by one of following approach:
		//  (1) (Context)->Draw({ .VertexCountPerInstance{ (Your index per instance count.) }, [.OtherParameter{ (value) }]... });
		//  (2) (Context)->Draw({ (Your index per instance count.), [ (values) ... ]});
		void Draw(DrawParameter const& parameter) const noexcept
		{
			m_CommandList->IASetPrimitiveTopology(parameter.Topology);
			m_CommandList->DrawInstanced(parameter.VertexCountPerInstance, parameter.InstanceCount, 
				parameter.StartVertexLocation, parameter.StartInstanceLocation);
		}

		// simply pack up.
		struct DrawIndexParameter 
		{
			
			::UINT IndexCountPerInstance{};
			::UINT InstanceCount              {1u};
			::UINT StartIndexLocation         {0u};
			::INT BaseVertexLocation          {0u};
			::UINT StartInstanceLocation      {0u};
			::D3D_PRIMITIVE_TOPOLOGY Topology {::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST};
		};
		// Instance count defualt at 1, Topology default at TRIANGLELIST.
		//  |w|
		// tips: You can invoke by one of following approach:
		//  (1) (Context)
		//      ->DrawIndex({ .IndexCountPerInstance{ (Your index per instance count.) }, [.OtherParameter{ (value) }]... });
		//  (2) (Context)->DrawIndex({ (Your index per instance count.), [(values ... )] })
		void DrawIndex(DrawIndexParameter const& parameter) noexcept
		{
			m_CommandList->IASetPrimitiveTopology(parameter.Topology);
			m_CommandList->DrawIndexedInstanced(parameter.IndexCountPerInstance, parameter.InstanceCount, 
				parameter.StartIndexLocation, parameter.BaseVertexLocation, parameter.StartInstanceLocation);
		}

	private:
		inner::Pointer<DescriptorHeap> m_DescriptorsRenderTarget{};
		inner::Pointer<DescriptorHeap> m_DescriptorsDepthStencil{};
	};
	// General direct graphics context.
	using DirectCommandContext = CommandContext::DirectCommandContext;

	class CommandContext::CopyCommandContext
		: public ContextCommon
		, CopyContext<CopyCommandContext>
		, BarrierContext<CopyCommandContext>
	{
		friend class CommandContext;
	public:

		TWEN_ISCA Type{ ::D3D12_COMMAND_LIST_TYPE_COPY };

		using ContextCommon::ContextCommon;
		using CopyContext::Copy;
		using BarrierContext::Transition;
		using BarrierContext::FlushBarriers;
	};
	// General copy command context.
	using CopyCommandContext = CommandContext::CopyCommandContext;

	// Do not use currently.
	class CommandContext::ComputeCommandContext 
		: public ContextCommon
		, CopyContext<ComputeCommandContext> 
		, BarrierContext<ComputeCommandContext>
		, BindableContext<ComputeCommandContext>
	{
		friend class CommandContext;
	public:
		TWEN_ISCA Type{ ::D3D12_COMMAND_LIST_TYPE_COMPUTE };

		using ContextCommon::ContextCommon;
		using BindableContext::Bind;
		using BarrierContext::Transition;
		using BarrierContext::FlushBarriers;

	private:

	};
	// General compute command context. (NOT FINISHED).
	using ComputeCommandContext = CommandContext::ComputeCommandContext;
}

namespace twen 
{
	template<typename Fn>
	inline void CommandContext::ExecuteOnMain(/*Device* device,*/ Fn&& fn)
		requires::std::invocable<Fn, CommandContext::DirectCommandContext*>
	{
		auto context = CreateNewContext<DirectCommandContext>(GetDevice());

		fn(context);

		Close(context);
	}

	template<typename Fn>
	inline void CommandContext::ExecuteOnBatch(/*Device* device,*/ Fn&& fn) requires
		::std::invocable<Fn, typename CommandContext::CopyCommandContext*>   ||
		::std::invocable<Fn, typename CommandContext::ComputeCommandContext*>||
		::std::invocable<Fn, typename CommandContext::DirectCommandContext*>
	{

		ContextCommon* commonContext;
		if constexpr (::std::invocable<Fn, CopyCommandContext*>)
		{
			auto context = CreateNewContext<CopyCommandContext>(GetDevice());
			fn(context);
			commonContext = context;
		} else if constexpr (::std::invocable<Fn, ComputeCommandContext*>) {
			static_assert(!::std::invocable<Fn, ComputeCommandContext*>, "Compute context is not finished yet...");

			auto context = CreateNewContext<ComputeCommandContext>(GetDevice());

			fn(context);
			commonContext = context;
		} else {
			auto context = CreateNewContext<DirectCommandContext>(GetDevice());

			fn(context);
			commonContext = context;
		}

		Close(commonContext);
	}

	inline CommandContext::~CommandContext()
	{
		// discard allocators.
		for (auto& [_, allocator] : m_Allocators)
		{
			allocator->Release();
		}
		// discard payloads.
		for (auto& [_, payloads] : m_Payloads)
		{
			for (auto& payload : payloads)
			{
				payload->Release();
			}
		}
		// dispose all state tracker.
		for (auto&& [resource, tracker] : m_States)
			delete tracker;
	}

	// For single thread is ok.
	inline void CommandContext::Commit()
	{
		GetDevice().m_ResidencyManager->StorageResident();

		for (auto& [type, state] : m_Payloads)
		{
			auto queue{ GetDevice().Queue(type) };
			queue->AddPayloads(::std::move(state));
			if (FAILED(queue->Wait(queue->Execute()))) 
				MODEL_ASSERT(false, "Wait failure.");
			m_Allocators.at(type)->Reset();
		}

		for (auto& activeContext : m_ActiveContexts) 
		{
			switch (activeContext->Type)
			{
			case D3D12_COMMAND_LIST_TYPE_DIRECT:
				delete static_cast<DirectCommandContext*>(activeContext);
				break;
			case D3D12_COMMAND_LIST_TYPE_COMPUTE:
				delete static_cast<ComputeCommandContext*>(activeContext);
				break;
			case D3D12_COMMAND_LIST_TYPE_COPY:
				delete static_cast<CopyCommandContext*>(activeContext);
				break;
			case D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE:
				// Reserved.
				delete activeContext;
				break;
			case D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS:
				// Reserved.
				delete activeContext;
				break;
			case D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE:
				// Reserved.
				delete activeContext;
				break;
			default:MODEL_ASSERT(false, "Unknown type.");
				break;
			}
		}
		m_ActiveContexts.clear();

		m_Payloads.clear();
		if (m_States.size())
			::std::erase_if(m_States, 
				[](auto const& pair) 
				{
					if (!pair.second->NumNotIdle)
					{
						delete pair.second;
						return true;
					} else 
						return false;
				});
	}

	inline void CommandContext::Close(ContextCommon* common)
	{
		auto hr{ common->m_CommandList->Close() };
		MODEL_ASSERT(SUCCEEDED(hr), "Close command list failure.");

		m_ActiveContexts.emplace_back(common);
		m_Payloads[common->Type].emplace_back(common->m_CommandList);
	}
}