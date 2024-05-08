#pragma once

namespace twen
{
	//namespace CommandList
	//{
	//	// State describe GraphicsCommandList.
	//	struct State
	//	{
	//		bool		Opened : 1		{false};	// Command list is opened.
	//		bool		Submitted : 1	{false};	// Commands were submmitted.
	//		bool		QueryEnded : 1	{false};	// QuerySet was finished.
	//		bool		QueryBegun : 1	{false};	// QuerySet was begun.
	//		void Reset()
	//		{
	//			Opened = false;
	//			Submitted = false;
	//			QueryEnded = false;
	//			QueryBegun = false;
	//		}
	//	};
	//}

	class GraphicsPipelineState;
	class ComputePipelineState;
	class QuerySet;
	class DescriptorSet;
	class RootSignature;
	class VertexBuffer;
	class IndexBuffer;

	namespace CommandList 
	{
		using Type =::ID3D12GraphicsCommandList4;
	}

	struct DECLSPEC_NOVTABLE Context
	{
		friend class Queue;
		friend class CommandContextSet;
	public:
		using sort_t = ::D3D12_COMMAND_LIST_TYPE;

		virtual ~Context() = default;
	public:
		template<typename ResourceT>
		FORCEINLINE void Transition(::std::shared_ptr<ResourceT> resource, ::D3D12_RESOURCE_STATES newState, auto&&...args)
		{ 
			::D3D12_RESOURCE_BARRIER barrier = resource->Transition(newState, ::std::forward<decltype(args)>(args)...);
			if(barrier.Transition.StateAfter != barrier.Transition.StateBefore)
				m_Barriars.push_back(::std::move(barrier)); 
		}

		template<typename ResourceT>
		FORCEINLINE void Alias(::std::shared_ptr<ResourceT> resource, ::std::shared_ptr<ResourceT> other, auto&&...args)
		{ m_Barriars.push_back(resource->Aliasing(other, ::std::forward<decltype(args)>(args)...)); }

		void UAVBarrier(auto&&...UNFINISHED);

		FORCEINLINE void FlushBarriars()
		{ CommandList->ResourceBarrier(static_cast<::UINT>(m_Barriars.size()), m_Barriars.data()), m_Barriars.clear(); }

		void BeginQuery(Pointer<QuerySet> const& query);
		void EndQuery(Pointer<QuerySet> const& query);

		void Wait(::std::shared_ptr<Queue> queue, ::UINT64 value);

		template<DerviedFrom<Context> ContextT> 
		ContextT& As() { assert(Type == ContextT::Type && "Wrong cast."); 
						 return static_cast<ContextT&>(*this); }
	public:
		const sort_t Type;
		CommandList::Type* const CommandList; // Do not call release directly.
	protected:
		Context(CommandList::Type* listToRecord, sort_t type)
			: Type{type}, CommandList{listToRecord} {}
	protected:
		::std::vector<::D3D12_RESOURCE_BARRIER> m_Barriars;
	};

	struct ComputeContext;
	struct DirectContext : Context
	{
	public:
		static constexpr auto Type{ ::D3D12_COMMAND_LIST_TYPE_DIRECT };
	public:
		// TODO: This expect the pipeline state object's rootsignature is just right for actual resource binding within pipeline state. Which is might causing wasting. 
		//	METHOD: Complete shader reflection part, replace it with shader status in pipeline.
		DirectContext(CommandList::Type* listToRecord,
			::std::shared_ptr<GraphicsPipelineState> pso, ::std::shared_ptr<RootSignature> rso,
			::std::shared_ptr<DescriptorSet> rtvSet, ::std::shared_ptr<DescriptorSet> dsvSet,
			::std::shared_ptr<DescriptorSet> csuSet, ::std::shared_ptr<DescriptorSet> samplerSet);

		DirectContext(CommandList::Type* listToRecord)
			: Context{listToRecord, Type}
		{} // Create as an copy queue.

		DirectContext(DirectContext const& upperContext);
		~DirectContext();
	public:
		void Viewport(::D3D12_RECT viewPort);

		//::std::unique_ptr<DirectContext> SwitchContext(::std::shared_ptr<GraphicsPipelineState>);
		//::std::unique_ptr<ComputeContext> SwitchContext(::std::shared_ptr<ComputePipelineState>);

		template<typename Bindable>
		FORCEINLINE void Bind(::std::shared_ptr<Bindable> bindable, auto&&...args)
			//requires requires { bindable->Bind(*this, ::std::forward<decltype(args)>(args)...); }
		{ bindable->Bind(*this, ::std::forward<decltype(args)>(args)...); }

		template<typename CopyOperator>
		FORCEINLINE void Copy(::std::shared_ptr<CopyOperator> copyable, auto&&...args) 
		{ copyable->Copy(*this, ::std::forward<decltype(args)>(args)...); }

		void BeginRenderPass(const float rtvClearValue[4], ::D3D12_DEPTH_STENCIL_VALUE dsvClearValue = {}, 
			bool clearDepth = false, bool clearStencil = false);

		void BeginDescriptorTable(bool isSampler = false);
		void EndDescriptorTable(::UINT rootIndex, bool isSampler = false);
		// Early access.
		void Draw(::D3D_PRIMITIVE_TOPOLOGY topology, ::UINT vertexCountOrNumIndexPerVertex, ::UINT instanceCount = 1u);

		void EndRenderPass();

	#if D3D12_MODEL_DEBUG
		::D3D12_ROOT_PARAMETER_TYPE CheckBeforeBinding(::UINT index);

	#define BINDING_CHECK(contextName, index, exceptedType) \
	assert(contextName.CheckBeforeBinding(index)==static_cast<::D3D12_ROOT_PARAMETER_TYPE>(exceptedType) && "Type mismatched.");
	#else
	#define	BINDING_CHECK(...)
	#endif

	public:
		Pointer<DescriptorSet> RTVHandles{};
		Pointer<DescriptorSet> DSVHandle{};
		Pointer<DescriptorSet> CSUHandles{};
		Pointer<DescriptorSet> SamplerHandles{};

		::std::vector<::D3D12_VERTEX_BUFFER_VIEW> VerticesView;
		::D3D12_INDEX_BUFFER_VIEW IndicesView{};

		::std::weak_ptr<RootSignature> BindedRootSignature{};

		//::std::weak_ptr<VertexBuffer> VertexBuffer;
		//::std::weak_ptr<IndexBuffer>  IndexBuffer;

		bool RenderTargetWasSet : 1		{};
		bool ViewPortWasSet : 1			{};
		bool BeginRecordingTable : 1	{};
		bool BeginSamplerTable : 1		{};
	private:
		::UINT TableIndex{};
		::std::weak_ptr<GraphicsPipelineState> GraphicsState;
	};

	class Texture;
	class TextureCopy;

	struct DECLSPEC_EMPTY_BASES CopyContext : Context
	{
	public:
		static constexpr auto Type{ ::D3D12_COMMAND_LIST_TYPE_COPY };
	public:
		CopyContext(CommandList::Type* listToRecord)
			: Context{listToRecord, Type} {}
	public:
		//void CopyTexture(::std::shared_ptr<TextureCopy> copyBuffer);
		//void CopyTexture(::std::shared_ptr<Texture> dst, ::std::shared_ptr<Texture> src);
		template<typename CopyOperator>
		FORCEINLINE void Copy(::std::shared_ptr<CopyOperator> copyable, auto&&...args)
		{ copyable->Copy(*this, ::std::forward<decltype(args)>(args)...); }
	};
	
	struct ComputeContext : public Context 
	{
	public:

	public:
		::std::weak_ptr<ComputePipelineState> m_StateCompute;
		::std::weak_ptr<RootSignature> RootSignature;
		Pointer<DescriptorSet> m_IndirectArgument;
	};

	class CommandBundle 
	{
	public:

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
			device->CreateCommandAllocator(Type, IID_PPV_ARGS(m_Allocator.put()));
			assert(m_Allocator && "Create compute command allocator failure.");
			SET_NAME(m_Allocator, L"CommandAllocator");
		}

		template<DerviedFrom<Context> ContextT, typename...Ts>
		FORCEINLINE::std::unique_ptr<ContextT> Begin(Ts&&...args)
			requires::std::constructible_from<ContextT, CommandList::Type*, Ts...> 
		{
			assert(ContextT::Type == this->Type && "Type mismatch.");
			assert(!IsOpen && "Must not in open state.");

			if (Submitted) Reset();

			auto& device{ GetDevice() };
			CommandList::Type* commandList;
			device->CreateCommandList(device.NativeMask, ContextT::Type, m_Allocator.get(), nullptr, IID_PPV_ARGS(&commandList));
			assert(commandList && "Create command list failure.");

			IsOpen = true;
			return::std::make_unique<ContextT>(commandList, ::std::forward<Ts>(args)...);
		}
		
		// helper.
		FORCEINLINE void Close(::std::unique_ptr<DirectContext>& context) { Close(::std::move(context)); }
		FORCEINLINE void Close(::std::unique_ptr<CopyContext>& context) { Close(::std::move(context)); }
		FORCEINLINE void Close(::std::unique_ptr<ComputeContext>& context) { Close(::std::move(context)); }

		FORCEINLINE void Close(::std::unique_ptr<Context>&& context) 
		{
			assert(IsOpen && "Haven't begun.");
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
		FORCEINLINE void Reset() 
		{
			//assert(!m_ExecutionQueue.expired() && "Execution queue is empty.");
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
		// TODO: ::std::atomic<bool>...
		bool IsOpen{false};
		bool Submitted{false};

		::UINT64 m_LastTicket;
		ComPtr<::ID3D12CommandAllocator> m_Allocator;
		//::std::unordered_map<::D3D12_COMMAND_LIST_TYPE, ComPtr<::ID3D12CommandAllocator>> m_Allocators;

		::std::vector<::std::unique_ptr<Context>> m_Contexts;
		::std::vector<::ID3D12CommandList*> m_Payloads;
		::std::weak_ptr<Queue> m_ExecutionQueue;
	};
}