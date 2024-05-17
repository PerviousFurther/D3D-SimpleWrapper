#pragma once

namespace twen
{
	namespace Bindings 
	{
		template<typename> struct Bindable;
	}

	class GraphicsPipelineState;
	class ComputePipelineState;
	class QuerySet;
	class DescriptorSet;
	class RootSignature;
	class VertexBuffer;
	class IndexBuffer;

	namespace CommandLists 
	{
		using Type =::ID3D12GraphicsCommandList4;
	}

	struct DECLSPEC_NOVTABLE Context
	{
		friend class Queue;
		friend class CommandContextSet;
		template<typename T> friend struct Bindings::Bindable;
	public:
		using sort_t = ::D3D12_COMMAND_LIST_TYPE;

		virtual ~Context() = default;
	public:
		template<typename ResourceT>
		inline void Transition(::std::shared_ptr<ResourceT> resource, ::D3D12_RESOURCE_STATES newState, auto&&...args)
		{ 
			::D3D12_RESOURCE_BARRIER barrier = resource->Transition(newState, ::std::forward<decltype(args)>(args)...);
			if(barrier.Transition.StateAfter != barrier.Transition.StateBefore)
				m_Barriars.push_back(::std::move(barrier)); 
		}

		template<typename ResourceT>
		inline void Alias(::std::shared_ptr<ResourceT> resource, ::std::shared_ptr<ResourceT> other, auto&&...args)
		{ m_Barriars.push_back(resource->Aliasing(other, ::std::forward<decltype(args)>(args)...)); }

		void UAVBarrier(auto&&...UNFINISHED);

		inline void FlushBarriars()
		{ CommandList->ResourceBarrier(static_cast<::UINT>(m_Barriars.size()), m_Barriars.data()), m_Barriars.clear(); }

		void BeginQuery(Pointer<QuerySet> const& query);
		void EndQuery(Pointer<QuerySet> const& query);

		void Wait(::std::shared_ptr<Queue> queue, ::UINT64 value);

		template<inner::congener<Context> ContextT> 
		ContextT& As() { assert(Type == ContextT::Type && "Wrong cast."); 
						 return static_cast<ContextT&>(*this); }
	public:
		const sort_t Type;
		CommandLists::Type* const CommandList; // Do not call release directly.
	protected:
		Context(CommandLists::Type* listToRecord, sort_t type)
			: Type{type}, CommandList{listToRecord} {}
	protected:
		::std::vector<::D3D12_RESOURCE_BARRIER> m_Barriars;
	};

	struct ComputeContext;
	struct DirectContext : Context
	{
		template<typename T> friend struct Bindings::Bindable;
		template<typename T> friend class ConstantBuffer;

		// Remove in future.
		friend class Texture;
		friend class Buffer;
		friend class VertexBuffer;
		friend class IndexBuffer;

		friend class RenderBuffer;
		friend class DepthStencil;
	public:
		static constexpr auto Type{ ::D3D12_COMMAND_LIST_TYPE_DIRECT };
	public:
		DirectContext(CommandLists::Type* listToRecord,
			::std::shared_ptr<GraphicsPipelineState> pso, ::std::vector<::std::shared_ptr<DescriptorSet>> const& sets);

		DirectContext(CommandLists::Type* listToRecord, 
			::std::shared_ptr<RootSignature> rso, ::std::shared_ptr<GraphicsPipelineState> pso,
			::std::vector<::std::shared_ptr<DescriptorSet>> const& descriptorSets);

		DirectContext(CommandLists::Type* listToRecord)
			: Context{listToRecord, Type}
		{} // Create as an copy queue.

		DirectContext(DirectContext const& upperContext);
		~DirectContext();
	public:
		void Viewport(::D3D12_RECT viewPort);

		template<typename Bindable>
		inline void Bind(::std::shared_ptr<Bindable> bindable, auto&&...args)
			//requires requires { bindable->Bind(*this, ::std::forward<decltype(args)>(args)...); }
		{ bindable->Bind(*this, ::std::forward<decltype(args)>(args)...); }

		template<typename CopyOperator>
		inline void Copy(::std::shared_ptr<CopyOperator> copyable, auto&&...args) 
		{ copyable->Copy(*this, ::std::forward<decltype(args)>(args)...); }

		void BeginRenderPass(const float rtvClearValue[4], ::D3D12_DEPTH_STENCIL_VALUE dsvClearValue = {}, 
			bool clearDepth = false, bool clearStencil = false);

		//void BeginDescriptorTable(bool isSampler = false);
		//void EndDescriptorTable(::UINT rootIndex, bool isSampler = false);

		void Draw(::D3D_PRIMITIVE_TOPOLOGY topology, ::UINT vertexCountOrNumIndexPerVertex, ::UINT instanceCount = 1u);

		void EndRenderPass();
	private:
		void LinkPipeline(::std::shared_ptr<GraphicsPipelineState> pso);


		struct Agreement 
		{
			~Agreement() 
			{
				if (List) 
					List->SetGraphicsRootDescriptorTable(Root, PositionGPU);
			}
			operator::D3D12_CPU_DESCRIPTOR_HANDLE() const 
			{
				return PositionCPU;
			}
		
			::D3D12_CPU_DESCRIPTOR_HANDLE PositionCPU;
			CommandLists::Type* List;
			::UINT Root;
			::D3D12_GPU_DESCRIPTOR_HANDLE PositionGPU;
		};

		using HandleCpu = Agreement;
		using HandleGpu = ::D3D12_GPU_DESCRIPTOR_HANDLE;

		bool IsOnDescriptorTable(::UINT index) const 
		{ assert(m_Roots.size() > index); return m_Roots.at(index).Type == ::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; }
		Agreement ObtainHandle(::UINT index, ::UINT indexOfRange)
		{
			auto& bind{ m_Roots.at(index) };
			if (bind.Type !=::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) 
			{
				MODEL_ASSERT(false, "Not an table.");
				return {};
			} else {
				MODEL_ASSERT(bind.Ranges.size() > indexOfRange, "Out of range.");

				auto& range{ bind.Ranges.at(indexOfRange) };
				const bool isSampler{ range.Type == ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER };

				MODEL_ASSERT(range.BindedInput, "This range wasnt bound by any input.");

				auto& input{ *range.BindedInput };

				auto const& handle = m_Handles.at(isSampler ?
					::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				
				return{ handle[input.Space + input.BindCount++], input.BindCount == range.Count ? CommandList : nullptr, input.BindPoint, handle(input.Space) };
			}
		}

		::D3D12_CPU_DESCRIPTOR_HANDLE ObtainHandle(::D3D12_DESCRIPTOR_HEAP_TYPE type, ::UINT index)
		{
			MODEL_ASSERT(m_Handles.contains(type), "Specified Type is not existing.");
			return m_Handles.at(type)[index];
		}
		bool IsOnDescriptorTable(::std::string_view name)
		{
			MODEL_ASSERT(m_Binds.contains(name), "Specifed name is not found. Check if the name is miswrite with lower case or upper case.");
			return IsOnDescriptorTable(m_Binds.at(name).BindPoint);
		}
		::UINT ObtainRoot(::std::string_view name)
		{
			MODEL_ASSERT(m_Binds.contains(name), "Didnt contain spciefied named object.");

			return m_Binds.at(name).BindPoint;
		}
		// Obtain descriptor handle's position by name in shader's input to create view.
		Agreement ObtainHandle(::std::string_view name)
		{
			if (m_Binds.contains(name))
			{
				auto& input{ m_Binds.at(name) };
				const bool isSampler{ input.Type == ::D3D_SIT_SAMPLER };

				MODEL_ASSERT(input.BindedRange, "Input didnt bind a range.");
				MODEL_ASSERT(input.BindedRange->Count > input.BindCount, "Bind too many resource on point.");

				auto const& handle = m_Handles.at(isSampler ?
					::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				return{ handle[input.Space + input.BindCount++], (input.BindCount == input.BindedRange->Count) ? CommandList : nullptr, input.BindPoint, handle(input.Space) };
			}

			MODEL_ASSERT(false, "Didnt contains named bind point.");

			return {};
		}
	private:
		::UINT m_NumRenderTarget;
		bool m_EnableDepth;			// from pipeline.
		::std::vector<Descriptors::Root> m_Roots;
		::std::vector<Descriptors::Static> m_StaticSampler;

		::std::unordered_map<::std::string_view, Shaders::Input> m_Binds;
		::std::unordered_map<::D3D12_DESCRIPTOR_HEAP_TYPE, Pointer<DescriptorSet>> m_Handles;
	};

	class Texture;
	class TextureCopy;

	struct DECLSPEC_EMPTY_BASES CopyContext : Context
	{
		template<typename T> friend struct Bindings::Bindable;
	public:
		static constexpr auto Type{ ::D3D12_COMMAND_LIST_TYPE_COPY };
	public:
		CopyContext(CommandLists::Type* listToRecord)
			: Context{listToRecord, Type} {}
	public:
		template<typename CopyOperator>
		inline void Copy(::std::shared_ptr<CopyOperator> copyable, auto&&...args)
		{ copyable->Copy(*this, ::std::forward<decltype(args)>(args)...); }
	};
	
	struct ComputeContext : public Context 
	{
		template<typename T> friend struct Bindings::Bindable;
	public:

	public:
		::std::weak_ptr<ComputePipelineState> m_StateCompute;
		::std::weak_ptr<RootSignature> RootSignature;
		Pointer<DescriptorSet> m_IndirectArgument;
	};
}

namespace twen
{
	// Each manager repersented an thread, in ideal situation.
	class CommandContextSet : public::std::enable_shared_from_this<CommandContextSet>, public DeviceChild
	{
		friend class Queue;
	public:
		using sort_t =::D3D12_COMMAND_LIST_TYPE;
	public:
		CommandContextSet(Device& device, sort_t type)
			: DeviceChild{device}, Type{type}
		{
			device->CreateCommandAllocator(Type, IID_PPV_ARGS(m_Allocator.Put()));
			assert(m_Allocator && "Create compute command allocator failure.");
			SET_NAME(m_Allocator, L"CommandAllocator");
		}

		template<inner::congener<Context> ContextT, typename...Ts>
		inline::std::unique_ptr<ContextT> Begin(Ts&&...args)
			requires::std::constructible_from<ContextT, CommandLists::Type*, Ts...> 
		{
			assert(ContextT::Type == this->Type && "Type mismatch.");
			assert(!IsOpen && "Must not in open state.");

			if (Submitted) Reset();

			auto& device{ GetDevice() };
			CommandLists::Type* commandList;
			device->CreateCommandList(device.NativeMask, ContextT::Type, m_Allocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
			assert(commandList && "Create command list failure.");

			IsOpen = true;
			return::std::make_unique<ContextT>(commandList, ::std::forward<Ts>(args)...);
		}
		
		// helper.
		inline void Close(::std::unique_ptr<DirectContext>& context) { Close(::std::move(context)); }
		inline void Close(::std::unique_ptr<CopyContext>& context) { Close(::std::move(context)); }
		inline void Close(::std::unique_ptr<ComputeContext>& context) { Close(::std::move(context)); }

		inline void Close(::std::unique_ptr<Context>&& context) 
		{
			assert(IsOpen && "Haven't begun.");
			assert(context->Type == Type && "Commandlist's type is mismatched.");

			if(context->m_Barriars.size()) context->FlushBarriars();

			auto hr = context->CommandList->Close();
			if (FAILED(hr))
			{
			#if D3D12_MODEL_DEBUG
				auto message = GetDevice().Message();

				auto size = MultiByteToWideChar(CP_UTF8, NULL, message.data(), static_cast<int>(message.size()), nullptr, 0);
				::std::wstring wMessage(size, '\0');
				MultiByteToWideChar(CP_UTF8, NULL, message.data(), static_cast<int>(message.size()), wMessage.data(), static_cast<int>(message.size()));

				_wassert(wMessage.data(), __FILEW__, __LINE__);
			#else
				throw::std::exception("Inner error.");
			#endif
			}

			m_Payloads.push_back(context->CommandList);
			m_Contexts.push_back(::std::move(context));
			IsOpen = false;
		}
		inline void Reset() 
		{
			if (!m_ExecutionQueue.expired()) 
			{
				auto queue{ m_ExecutionQueue.lock() };
				queue->Wait(m_LastTicket);

				m_Allocator->Reset();
				m_Contexts.clear();

				m_ExecutionQueue.reset();
				Submitted = false;
			}
		}
		const sort_t Type;
	private:
		bool IsOpen{false};
		bool Submitted{false};

		::UINT64 m_LastTicket;
		ComPtr<::ID3D12CommandAllocator> m_Allocator;

		::std::vector<::std::unique_ptr<Context>> m_Contexts;
		::std::vector<::ID3D12CommandList*> m_Payloads;
		::std::weak_ptr<Queue> m_ExecutionQueue;
	};
}