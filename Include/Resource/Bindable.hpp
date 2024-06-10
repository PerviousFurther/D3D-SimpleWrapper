

namespace twen::Views
{
	inline auto ArrayIndex(::UINT16 mipLevels, ::UINT16 arraySize, ::UINT subresourceIndex)
	{
		return subresourceIndex / (mipLevels * arraySize);
	}

	struct Resource 
		: public inner::DeviceChild
	{
		// Let resource alloactor set pointer.
		friend struct::twen::ResourceAllocator;
	public:

		Resource(Resource const&) = delete;
		Resource& operator=(Resource const&) = delete;

		~Resource()
		{
			auto w = m_Name.size();
			m_Position.Fallback();
		}

		bool CanBind() const { return m_Position.Size; }

		template<typename View>
		::std::shared_ptr<View> As();

		::twen::Resource* BackingResource() const 
		{ 
			MODEL_ASSERT(CanBind(), "try obtain invaild resource."); 
			return m_Position.Backing; 
		}

		inner::Pointer<::twen::Resource> const& ResourcePosition() const { return m_Position; }

		bool IsTexture() const { return !m_Position.Address; }
		bool IsBuffer()  const { return !!m_Position.Address; }

		::D3D12_GPU_VIRTUAL_ADDRESS Address() const 
		{ 
			MODEL_ASSERT(m_Position.Address, "Only buffer can get address.");
			MODEL_ASSERT(CanBind(), "Pointer was not set.");

			return m_Position.Address;
		}

		operator::ID3D12Resource* () const { return *m_Position.Backing; }

	public:
	#if D3D12_MODEL_DEBUG
		const Views::Type Type;
	#endif
	protected:
		Resource(Device& device, Views::Type type, ::std::string_view name)
			: DeviceChild{ device }
		#if D3D12_MODEL_DEBUG
			, Type{type}
			, m_Name{name}
		#endif
		{}
	#if D3D12_MODEL_DEBUG
		::std::string m_Name;
	#endif
		twen::inner::Pointer<twen::Resource> m_Position{};
	};

	struct DECLSPEC_EMPTY_BASES empty_base {};

	template<typename View>
	concept is_srv = ::std::convertible_to<typename View::view_t, ::D3D12_SHADER_RESOURCE_VIEW_DESC>;

	template<typename View>
	concept is_uav = ::std::convertible_to<typename View::view_t, ::D3D12_UNORDERED_ACCESS_VIEW_DESC>;

	template<typename View>
	concept is_rtv = ::std::convertible_to<typename View::view_t, ::D3D12_RENDER_TARGET_VIEW_DESC>;

	template<typename View>
	concept is_dsv = ::std::convertible_to<typename View::view_t, ::D3D12_DEPTH_STENCIL_VIEW_DESC>;

	template<typename View>
	concept is_ibv = ::std::convertible_to<typename View::view_t, ::D3D12_INDEX_BUFFER_VIEW>;

	template<typename View>
	concept is_vbv = ::std::convertible_to<typename View::view_t, ::std::vector<::D3D12_VERTEX_BUFFER_VIEW>>;

	template<typename View, typename Ext>
	void InitView(::std::shared_ptr<Bindable<View, Ext>>* view);

	template<typename View, typename Extension>
	class Bindable
		: public Views::Resource
		, public::std::conditional_t<::std::is_null_pointer_v<Extension>, empty_base, Extension>
		, public inner::ShareObject<Bindable<View, Extension>>
	{
		friend class Device;
	public:
		using view = View;
		using desc_t = ResourceOrder;
		using extension = Extension;
		using view_desc = typename view::view_t;

		// for some view that combine with texture...
		Bindable(Device& device, desc_t const& desc, ::std::string_view name) requires 
			(!requires { view::always_upload; })
			: Resource{ device, view::ViewType, name }
		{ m_View.ViewDimension = view::TypeFromDesc(desc); }

		// for buffer only view.
		Bindable(Device& device, ::std::string_view name) requires 
			requires { view::always_upload; }
			: Resource{ device, view::ViewType, name }
		{}

		view_desc& View() requires (!requires(Extension e){ e.View(); }) { return m_View; }

		void Format(::DXGI_FORMAT format) requires 
			(requires { view::view_t::Format; } 
			|| is_ibv<View>)
		{
			if constexpr (requires { view::VerifyFormat(m_View.ViewDimension, format); })
				view::VerifyFormat(m_View.ViewDimension, format);

			m_View.Format = format;
		}

		// Dont recommand to modify this component.
		// beside u need to decode video or sth else.
		void Shader4Component(::UINT mapping) requires
			is_srv<view>
		{ m_View.Shader4ComponentMapping = mapping; }

		// Set structured buffer stride.
		void Stride(::UINT stride) noexcept(!D3D12_MODEL_DEBUG) requires
			(is_uav<view> || is_srv<view>) 
		{
			MODEL_ASSERT(static_cast<::UINT>(m_View.ViewDimension) == 1, "Only buffer can have structure byte stride.");
			m_View.Buffer.StructureByteStride = stride;
		}
		// Set vertex buffer stride.
		void Stride(::UINT sizeInByte) noexcept(!D3D12_MODEL_DEBUG) requires
			is_vbv<View>
		{
			MODEL_ASSERT(sizeInByte < m_Position.Size, "Stride too large.");
			m_View.StrideInBytes = sizeInByte;
		}
		// Set flags of buffer
		void BufferFlags(::std::conditional_t<is_srv<view>,
			::D3D12_BUFFER_SRV_FLAGS, ::D3D12_BUFFER_UAV_FLAGS> flags) noexcept(!D3D12_MODEL_DEBUG) requires
			(is_uav<view> || is_srv<view>)
		{
			MODEL_ASSERT(static_cast<::UINT>(m_View.ViewDimension) == 1, "Only buffer can have flags.");
			m_View.Buffer.Flags = flags;
		}
		// Set most detailed mipmap index.
		void MostDetailedMip(::UINT value) noexcept(!D3D12_MODEL_DEBUG) requires
			is_srv<view>
		{
			MODEL_ASSERT(static_cast<::UINT>(m_View.ViewDimension) != 1, "Buffer wont have mip levels.");
			m_View.Texture1D.MostDetailedMip = value;
		}
		// Set most 'mip' mipmap index.
		void MipLevels(::UINT value) noexcept(!D3D12_MODEL_DEBUG) requires
			is_srv<view>
		{
			MODEL_ASSERT(static_cast<::UINT>(m_View.ViewDimension) != 1, "Buffer wont have mip levels.");
			MODEL_ASSERT(value == 0u, "Cannot set miplevel to 0.");
			MODEL_ASSERT(m_View.Texture1D.MipLevels == 0u, "Resource was not initialized.");

			m_View.Texture1D.MipLevels = value;
		}

	private:
		void Pointer(inner::Pointer<::twen::Resource> && pointer)
		{
			m_Position = ::std::move(pointer);

			if constexpr (requires { this->InitView(); })
				this->InitView();
			else 
				InitView(this);
		}

		view_desc m_View{};
		using Views::Resource::m_Position;
	};
	
	template<typename View>
	inline::std::shared_ptr<View> Views::Resource::As()
	{
		MODEL_ASSERT(this->Type == View::view::ViewType, "Bad cast.");

		return static_cast<View*>(this)->share::shared_from_this();
	}

	//

	//


	//
#pragma warning(disable: 4063)
	//		For Textures...
	template<typename View, typename Ext> requires 
		(!requires{ View::always_upload; })
	void InitView(Bindable<View, Ext>* viewV)
	{
		auto& view = viewV->View();
		auto const& pointer = viewV->ResourcePosition();
		auto const& resource = viewV->BackingResource();
		view.Format = resource->Desc.Format;
		
		if constexpr (is_srv<View>)
		{
			view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		};

#define TWEN_MIP_SLICE(name)\
if constexpr( requires{ view.name; }){\
	if constexpr (is_srv<View>)\
	{\
		if constexpr (requires{view.name.MostDetailedMip;}) view.name.MostDetailedMip = 0u;\
		if constexpr (requires{view.name.MipLevels;}) view.name.MipLevels = pointer.MipLevels;\
		if constexpr (requires{view.name.ResourceMinLODClamp;}) view.name.ResourceMinLODClamp = 0.0f;\
	} else {\
		if constexpr (requires{view.name.MipSlice;}) view.name.MipSlice = 0u;\
	}}

#define TWEN_ARRAY_SLICE(name)\
	if constexpr( requires{ view.name; }){ \
		if constexpr (requires{ view.name.FirstArraySlice; }){\
		view.name.FirstArraySlice = pointer.ArrayIndex;\
		view.name.ArraySize = (::std::min)(pointer.NumSubresource / pointer.MipLevels, 1u);\
	}}
#define TWEN_PLANE(name)\
if constexpr( requires{ view.name; }){\
	if constexpr( requires{ view.name.PlaneSlice;}){ \
		view.name.PlaneSlice = 0u;\
	}}
#define TWEN_TABLE(value, name)\
		case value: { TWEN_MIP_SLICE(name) TWEN_ARRAY_SLICE(name) TWEN_PLANE(name) } break;

		switch (view.ViewDimension)
		{
		case 1: // BUFFER
		{
			if constexpr (requires{ view.Buffer; })
			{
				auto formatStride{ Utils::GetBitPerPixel(view.Format) >> 3 };
				MODEL_ASSERT(formatStride, "Buffer needs format.");

				view.Buffer.FirstElement = static_cast<::UINT>(pointer.Offset / formatStride);
				view.Buffer.NumElements = static_cast<::UINT>(pointer.Size / formatStride);
			} else MODEL_ASSERT(false, "Depth stencil cannot have buffer.");
		}break;

		case 8: // TEXTURE3D
			if constexpr (is_dsv<View>) 
			{
				MODEL_ASSERT(false, "Dsv cannot be Tex3D.");
				break;
			} else if constexpr (is_srv<View>) {
				view.Texture3D = {0u, 0u, 0.0f};
				break;
			} else if constexpr (is_rtv<View>) {
				// Not right.
				view.Texture3D.FirstWSlice = pointer.ArrayIndex;
				view.Texture3D.WSize = pointer.NumSubresource / pointer.MipLevels;
				break;
			}
			
		TWEN_TABLE(5, Texture2DArray);
		TWEN_TABLE(3, Texture1DArray);
		TWEN_TABLE(7, Texture2DMSArray);
		
		case 10: MODEL_ASSERT(false, "Texture cube should call on extension's InitView()."); break;

		TWEN_TABLE(4, Texture2D);
		TWEN_TABLE(2, Texture1D);
		case 6:break;

		TWEN_TABLE(9, TextureCube);

		case 11: //RAYTRACING_ACCELERATION_STRUCTURE:
		if constexpr (is_srv<View>)
		{
			MODEL_ASSERT(false, "Ray tracing is not finished.");
		break;
		}
		default:
		MODEL_ASSERT(false, "Unknown view dimension.");
			break;
		}
	}

#undef TWEN_MIP_SLICE
#undef TWEN_PLANE
#undef TWEN_TABLE
#undef TWEN_ARRAY_SLICE
#pragma warning(default: 4063)

	//

	//
	
	//
	
	//							For buffers.


	//



	//
	template<typename View, typename Ext> requires 
		requires{ View::always_upload; }
	void InitView(Bindable<View, Ext>* viewV)
	{
		auto& view = viewV->View();
		auto const& pointer = viewV->ResourcePosition();

		view = {};
		if constexpr (requires { view.BufferLocation; }) 
		{
			view.BufferLocation = pointer.Address;
			view.SizeInBytes = static_cast<::UINT>(pointer.Size);
		}
	}
}


//


namespace twen
{
	enum class ResourceCurrentStage
	{
		Write, Read, Idle, Copy,
	};
}


//


namespace twen
{

	//                  *                 =
	
	//         Render target view.        =
	
	//                  *                 =

	struct RenderTargetView
	{
		using desc_t = ::D3D12_RESOURCE_DESC;
		using view_t = ::D3D12_RENDER_TARGET_VIEW_DESC;
		using sort_t = ::D3D12_RTV_DIMENSION;

		struct render_target
		{};

		TWEN_ISCA CreateView { &::ID3D12Device::CreateRenderTargetView };
		TWEN_ISCA ViewType   { Views::RTV };

		TWEN_ISCA DescriptorHeapType { ::D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
		TWEN_ISCA DefaultView        { view_t{} };

		TWEN_ISCA HeapType    { ::D3D12_HEAP_TYPE_DEFAULT };
		TWEN_ISCA CommonState { ::D3D12_RESOURCE_STATE_RENDER_TARGET };

		TWEN_ISCA TypeFromDesc(::D3D12_RESOURCE_DESC const& desc) noexcept
		{
			switch (desc.Dimension)
			{
			case D3D12_RESOURCE_DIMENSION_BUFFER:
				return::D3D12_RTV_DIMENSION_BUFFER;
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				return
					desc.DepthOrArraySize > 1u
					? ::D3D12_RTV_DIMENSION_TEXTURE1DARRAY
					: ::D3D12_RTV_DIMENSION_TEXTURE1D;

			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				return
					desc.SampleDesc.Count > 1u
					? desc.DepthOrArraySize > 1
					? ::D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY
					: ::D3D12_RTV_DIMENSION_TEXTURE2DMS
					: desc.DepthOrArraySize > 1
					? ::D3D12_RTV_DIMENSION_TEXTURE2DARRAY
					: ::D3D12_RTV_DIMENSION_TEXTURE2D;
			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				return::D3D12_RTV_DIMENSION_TEXTURE3D;

			default:return::D3D12_RTV_DIMENSION_UNKNOWN;
			}
		}

	};

	template<>
	struct inner::Transition<RenderTargetView>
	{
		TWEN_ISCA WriteState{ ::D3D12_RESOURCE_STATE_RENDER_TARGET };
		TWEN_ISCA ReadState { ::D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE };

		TWEN_ISCA CopyState { ::D3D12_RESOURCE_STATE_RESOLVE_SOURCE | ::D3D12_RESOURCE_STATE_COPY_SOURCE | ::D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE };

		inline constexpr::D3D12_RESOURCE_STATES operator()(ResourceCurrentStage stage) const noexcept
		{
			switch (stage)
			{
			case ResourceCurrentStage::Write:
				return WriteState;

			case ResourceCurrentStage::Read:
				return ReadState;

			case ResourceCurrentStage::Copy:
				return CopyState;

			default:return::D3D12_RESOURCE_STATE_COMMON;
			}
		}
	};
	template<>
	struct inner::Operation<RenderTargetView, ::D3D12_COMMAND_LIST_TYPE_DIRECT>
	{
		using bindable = Views::Bindable<RenderTargetView>;
		using shared_bindable = ::std::shared_ptr<bindable>;

		// object render target.
		void operator()(shared_bindable view, ::D3D12_CPU_DESCRIPTOR_HANDLE handle) const
		{
			auto const& viewDesc = view->View();
			view->GetDevicePointer()->CreateRenderTargetView(*view, &viewDesc, handle);
		}
		// null render target.
		void operator()(Device& device, ::D3D12_CPU_DESCRIPTOR_HANDLE handle) const
		{
			static constexpr::D3D12_RENDER_TARGET_VIEW_DESC desc{};
			device->CreateRenderTargetView(nullptr, &desc, handle);
		}
	};
	template<>
	struct inner::ResourceCreationInfo<RenderTargetView>
	{
		TWEN_ISCA HeapFlags     { ::D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES };
		TWEN_ISCA ResourceFlags { ::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET };
		TWEN_ISCA HeapType{ ::D3D12_HEAP_TYPE_DEFAULT };
	};
	
	using RenderBuffer = Views::Bindable<RenderTargetView>;


				//                  *                 =
				
				//         Depth stencil view.        =
				
				//                  *                 =


	struct DepthStencilView
	{
		using desc_t = ::D3D12_RESOURCE_DESC;
		using view_t = ::D3D12_DEPTH_STENCIL_VIEW_DESC;
		using sort_t = ::D3D12_DSV_DIMENSION;

		TWEN_ISCA ViewType      { Views::DSV };

		TWEN_ISCA DefaultFormat { ::DXGI_FORMAT_D32_FLOAT };

		TWEN_ISCA DefaultView   { view_t{} };

		TWEN_ISCA CreateView         { &::ID3D12Device::CreateDepthStencilView };
		TWEN_ISCA DescriptorHeapType { ::D3D12_DESCRIPTOR_HEAP_TYPE_DSV };

		TWEN_ISCA TypeFromDesc(::D3D12_RESOURCE_DESC const& desc) noexcept
		{
			switch (desc.Dimension)
			{
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				return
					desc.DepthOrArraySize > 1
					? ::D3D12_DSV_DIMENSION_TEXTURE1DARRAY
					: ::D3D12_DSV_DIMENSION_TEXTURE1D;

			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				return
					desc.SampleDesc.Count > 1u
					? desc.DepthOrArraySize > 1u
					? ::D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY
					: ::D3D12_DSV_DIMENSION_TEXTURE2DMS
					: desc.DepthOrArraySize > 1u
					? ::D3D12_DSV_DIMENSION_TEXTURE2DARRAY
					: ::D3D12_DSV_DIMENSION_TEXTURE2D;

			default:return::D3D12_DSV_DIMENSION_UNKNOWN;
			}
		}
	};

	using DepthStencil = Views::Bindable<DepthStencilView>;

	template<>
	struct::twen::inner::Operation<DepthStencilView, ::D3D12_COMMAND_LIST_TYPE_DIRECT>
	{
		TWEN_ISCA CreateView{ &::ID3D12Device::CreateDepthStencilView };
		TWEN_ISCA Resolve   { &::ID3D12GraphicsCommandList::ResolveSubresource };

		void operator()(::std::shared_ptr<DepthStencil> depth, ::D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			depth->GetDevicePointer()->CreateDepthStencilView(*depth, &depth->View(), handle);
		}
	};

	template<>
	struct::twen::inner::Transition<DepthStencilView>
	{
		TWEN_ISCA WriteState { ::D3D12_RESOURCE_STATE_DEPTH_WRITE };
		TWEN_ISCA ReadState  { ::D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE };
		TWEN_ISCA CopyState  { ::D3D12_RESOURCE_STATE_DEPTH_READ | ::D3D12_RESOURCE_STATE_COPY_SOURCE };

		inline constexpr::D3D12_RESOURCE_STATES operator()(ResourceCurrentStage stage) const noexcept
		{
			switch (stage)
			{
			case ResourceCurrentStage::Write:
				return WriteState;

			case ResourceCurrentStage::Read:
				return ReadState;

			case ResourceCurrentStage::Copy:
				return CopyState;

			default:return::D3D12_RESOURCE_STATE_COMMON;
			}
		}

		// verify the state...?
		inline bool operator()(::D3D12_RESOURCE_STATES) const noexcept 
		{
			return true;
		}
	};

	template<>
	struct::twen::inner::ResourceCreationInfo<DepthStencilView>
	{
		TWEN_ISCA HeapFlags     { ::D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES };
		TWEN_ISCA ResourceFlags { ::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL };
		TWEN_ISCA HeapType{ ::D3D12_HEAP_TYPE_DEFAULT };
	};

	using DepthStencil = Views::Bindable<DepthStencilView>;

}





namespace twen 
{
	struct SwapchainBuffer
	{
	public:
		// will add reference count of swapchain.
		SwapchainBuffer(::IDXGISwapChain4* swapchain)
			: m_Swapchain{swapchain}
		{
			swapchain->AddRef();

			::DXGI_SWAP_CHAIN_DESC1 desc;
			m_Swapchain->GetDesc1(&desc);
			m_Resources.resize(desc.BufferCount);

			for (auto index{ 0u }; auto& resource : m_Resources) 
			{
				m_Swapchain->GetBuffer(index++, IID_PPV_ARGS(&resource));
				MODEL_ASSERT(resource, "Get buffer failure.");
			}
		}

		SwapchainBuffer(const SwapchainBuffer&) = delete;
		SwapchainBuffer operator=(SwapchainBuffer const&) = delete;

		~SwapchainBuffer() 
		{
			m_Swapchain->Release();
			for (auto& resource : m_Resources)
				resource->Release();
		}

		operator::ID3D12Resource* () const { return m_Resources.at(m_Swapchain->GetCurrentBackBufferIndex()); }

		::D3D12_RESOURCE_DESC Desc() 
		{
			auto desc{ m_Resources.front()->GetDesc() };
			return desc;
		}
	private:

		::IDXGISwapChain4* m_Swapchain;

		::std::vector<::ID3D12Resource*> m_Resources;
	};
}


						//                  *                   =
							    
						//         Shader resource view.        =
							    
						//                  *                   =



namespace twen
{
	struct DECLSPEC_EMPTY_BASES ShaderResourceView
	{
		using sort_t = ::D3D12_SRV_DIMENSION;
		using view_t = ::D3D12_SHADER_RESOURCE_VIEW_DESC;

		struct common_bind
		{};

		TWEN_ISCA ViewType{ Views::Type::SRV };
		TWEN_ISCA ClearValue{ (::D3D12_CLEAR_VALUE* const)nullptr };

		TWEN_ISCA HeapType{ ::D3D12_HEAP_TYPE_DEFAULT };

		TWEN_ISCA DescriptorHeapType{ ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

		TWEN_ISCA RootType{ ::D3D12_ROOT_PARAMETER_TYPE_SRV };
		TWEN_ISCA RangeType{ ::D3D12_DESCRIPTOR_RANGE_TYPE_SRV };

		TWEN_ISCA TypeFromDesc(::D3D12_RESOURCE_DESC const& desc) noexcept
		{
			switch (desc.Dimension)
			{
			case D3D12_RESOURCE_DIMENSION_BUFFER:
				// TODO: Buffer ex. buffer address is not supportted.
				return::D3D12_SRV_DIMENSION_BUFFER;
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				return
					desc.DepthOrArraySize > 1u
					? ::D3D12_SRV_DIMENSION_TEXTURE1DARRAY
					: ::D3D12_SRV_DIMENSION_TEXTURE1D;

			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				return
					desc.SampleDesc.Count > 1u
					? desc.DepthOrArraySize > 1u
					? ::D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY
					: ::D3D12_SRV_DIMENSION_TEXTURE2DMS
					: desc.DepthOrArraySize > 1u
					? ::D3D12_SRV_DIMENSION_TEXTURE2DARRAY
					: ::D3D12_SRV_DIMENSION_TEXTURE2D;

			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				return::D3D12_SRV_DIMENSION_TEXTURE3D;

			default:return::D3D12_SRV_DIMENSION_UNKNOWN;
			}
		}
	};

	template<::D3D12_COMMAND_LIST_TYPE type>
	struct::twen::inner::Operation<ShaderResourceView, type>
	{
		template<typename Ext>
		using shared_view = ::std::shared_ptr < Views::Bindable<ShaderResourceView, Ext> >;

		TWEN_ISCA Root
		{
		type == ::D3D12_COMMAND_LIST_TYPE_DIRECT
		? &::ID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView
		: &::ID3D12GraphicsCommandList::SetComputeRootShaderResourceView
		};
		TWEN_ISCA CreateView{ &::ID3D12Device::CreateShaderResourceView };

		template<typename Ext>
		void operator()(shared_view<Ext> view, ::ID3D12GraphicsCommandList* cmdlist, ::UINT root)
		{
			(cmdlist->*Root)(root, view->Address());
		}

		template<typename Ext>
		void operator()(shared_view<Ext> view, ::D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			(view->GetDevicePointer()->*CreateView)(*view, &view->View(), handle);
		}
	};

	template<>
	struct::twen::inner::Transition<ShaderResourceView>
	{
		TWEN_ISCA WriteState { ::D3D12_RESOURCE_STATE_COPY_DEST };
		TWEN_ISCA ReadState  { ::D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | ::D3D12_RESOURCE_STATE_COPY_SOURCE };
		TWEN_ISCA CopyState  { ::D3D12_RESOURCE_STATE_COMMON };

		inline constexpr::D3D12_RESOURCE_STATES operator()(ResourceCurrentStage stage) const
		{
			switch (stage)
			{
			case ResourceCurrentStage::Write:
				return WriteState;
			case ResourceCurrentStage::Read:
				return ReadState;
			case ResourceCurrentStage::Idle:
				return::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			default:return D3D12_RESOURCE_STATE_COMMON;
			}
		}
	};

	template<>
	struct::twen::inner::ResourceCreationInfo<ShaderResourceView>
	{
		TWEN_ISCA HeapFlags{ ::D3D12_HEAP_FLAG_NONE };
		TWEN_ISCA ResourceFlags{ ::D3D12_RESOURCE_FLAG_NONE };
		TWEN_ISCA HeapType{ ::D3D12_HEAP_TYPE_DEFAULT };
	};

}

TWEN_EXPORT namespace twen
{
	// Represent Structured buffer.
	using Structured = Views::Bindable<ShaderResourceView>;
	// Represent shader visible texture.
	using Texture = Views::Bindable<ShaderResourceView>;

	struct CubeExt 
	{
		using self_t = Views::Bindable<ShaderResourceView, CubeExt>;
		
		// Initialize the view after setting the pointer of resource.
		void InitView() 
		{
			auto self = static_cast<self_t*>(this);
			self->View().ViewDimension = ::D3D12_SRV_DIMENSION_TEXTURECUBE;

			inner::Pointer<Resource> const& position = self->ResourcePosition();
			MODEL_ASSERT(!position.Address, "Address should be empty.");
			MODEL_ASSERT(!(position.NumSubresource % 6u), "For texture cube, the subresource count should be multiple of 6.");


			ShaderResourceView::view_t& view = self->View();
			view.Format = position.Backing->Desc.Format;
			view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			if (position.NumSubresource > 6u) { // is texture cube.
				view.TextureCubeArray.First2DArrayFace = position.ArrayIndex;
				view.TextureCubeArray.NumCubes  = position.NumSubresource / position.MipLevels;
				view.TextureCubeArray.MipLevels = position.MipLevels;
				view.TextureCubeArray.MostDetailedMip = 0u;
				view.TextureCubeArray.ResourceMinLODClamp = 0.0f;
			} else { // is texture
				view.TextureCube.ResourceMinLODClamp = 0.0f;
				view.TextureCube.MipLevels = position.MipLevels;
				view.TextureCube.MostDetailedMip = 0u;
			}
		}

		// Revise desc's array size when creating texture 3d resource view.
		// Invoke time: (device).Create<TextureCube>({...});
		static void Revise(::D3D12_RESOURCE_DESC& desc) 
		{
			MODEL_ASSERT(desc.Dimension == ::D3D12_RESOURCE_DIMENSION_TEXTURE2D, "Must dimension 2d.");
			desc.DepthOrArraySize *= 6u;
		}

	protected:
		constexpr CubeExt() = default;
	};

	// Represent texture cube.
	// WARNING: 
	// When creating texture cube. Resource desc's array size will automatically set by following machism.
	//  (Resource desc's array size * 6u)
	using TextureCube = Views::Bindable<ShaderResourceView, CubeExt>;
}




						//                 *                    =
						
						//         Unordered access view.       =
						
						//                 *                    =





namespace twen
{
	struct UnorderedAccessView
	{
		using view_t = ::D3D12_UNORDERED_ACCESS_VIEW_DESC;
		using sort_t = ::D3D12_UAV_DIMENSION;

		struct common_bind{};
		
		TWEN_ISCA ViewType{ Views::Type::UAV };

		TWEN_ISCA DescriptorHeapType{ ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

		TWEN_ISCA RootType{ ::D3D12_ROOT_PARAMETER_TYPE_UAV };
		TWEN_ISCA RangeType{ ::D3D12_DESCRIPTOR_RANGE_TYPE_UAV };

		TWEN_ISCA TypeFromDesc(::D3D12_RESOURCE_DESC const& desc) noexcept
		{
			switch (desc.Dimension)
			{
			case D3D12_RESOURCE_DIMENSION_BUFFER:
				// TODO: Buffer ex. buffer address is not supportted.
				return::D3D12_UAV_DIMENSION_BUFFER;
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				return
					desc.DepthOrArraySize > 1u
					? ::D3D12_UAV_DIMENSION_TEXTURE1DARRAY
					: ::D3D12_UAV_DIMENSION_TEXTURE1D;

			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				return
					desc.SampleDesc.Count
					? D3D12_UAV_DIMENSION_UNKNOWN
					: desc.DepthOrArraySize > 1u
					? ::D3D12_UAV_DIMENSION_TEXTURE2DARRAY
					: ::D3D12_UAV_DIMENSION_TEXTURE2D;
			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				return::D3D12_UAV_DIMENSION_TEXTURE3D;

			default:return::D3D12_UAV_DIMENSION_UNKNOWN;
			}
		}
	};

	template<::D3D12_COMMAND_LIST_TYPE type>
	struct::twen::inner::Operation<UnorderedAccessView, type>
	{
		TWEN_ISCA Root
		{
		type == ::D3D12_COMMAND_LIST_TYPE_DIRECT
		? &::ID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView
		: &::ID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView
		};

		TWEN_ISCA CreateView{ &::ID3D12Device::CreateUnorderedAccessView };

		template<typename Ext>
		void operator()(::std::shared_ptr<Views::Bindable<UnorderedAccessView, Ext>> view, ::D3D12_CPU_DESCRIPTOR_HANDLE handle) 
		{
			if constexpr (::std::is_null_pointer_v<Ext>) 
				(view->GetDevicePointer()->*CreateView)(*view, nullptr, view->View(), handle);
			else 
				(view->GetDevicePointer()->*CreateView)(*view, view->Counter(), view->View(), handle);
		}
		// TODO: Check if the ext can be set on root.
		void operator()(::std::shared_ptr<Views::Bindable<UnorderedAccessView>> view, ::ID3D12GraphicsCommandList* cmdList, ::UINT root) 
		{
			(cmdList->*Root)(root, view->Address());
		}
	};

	// TODO: Write state cannot combine with other write state.
	template<>
	struct::twen::inner::Transition<UnorderedAccessView>
	{
		TWEN_ISCA WriteState { ::D3D12_RESOURCE_STATE_UNORDERED_ACCESS };
		TWEN_ISCA ReadState  { ::D3D12_RESOURCE_STATE_COMMON };
		TWEN_ISCA CopyState  { ::D3D12_RESOURCE_STATE_COPY_SOURCE };

		inline constexpr::D3D12_RESOURCE_STATES operator()(ResourceCurrentStage stage) const
		{
			switch (stage)
			{
			case ResourceCurrentStage::Write:
				return WriteState;

			case ResourceCurrentStage::Read:
				return ReadState;

			case ResourceCurrentStage::Copy:
				return CopyState;

			default:
				return::D3D12_RESOURCE_STATE_COMMON;
			}
		}
	};

	template<>
	struct::twen::inner::ResourceCreationInfo<UnorderedAccessView>
	{
		TWEN_ISCA HeapType      { ::D3D12_HEAP_TYPE_DEFAULT };
		TWEN_ISCA HeapFlags     { ::D3D12_HEAP_FLAG_NONE };
		TWEN_ISCA ResourceFlags { ::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS };
	};

	// Only appended structure buffer can be countered.
	struct Countered
	{
	public:
		::ID3D12Resource* Counter() const { return *m_Counter.Backing; }
	protected:
		void SetCounter(inner::Pointer<Resource> const& counter)
		{
			m_Counter = counter;
			static_cast<Views::Bindable<UnorderedAccessView, Countered>*>(this)->View().Buffer.CounterOffsetInBytes = counter.Offset;
		}

		inner::Pointer<Resource> m_Counter;
	};
}

TWEN_EXPORT namespace twen
{
	using RwBuffer = Views::Bindable<UnorderedAccessView>;

	using AppendStructured = Views::Bindable<UnorderedAccessView, Countered>;

	using RwTexture = Views::Bindable<UnorderedAccessView>;

}


namespace twen
{
	struct DECLSPEC_EMPTY_BASES SamplerView : inner::DeviceChild
	{
	public:
		TWEN_ISCA ViewType{ Views::Type::Sampler };

		using view_t = ::D3D12_SAMPLER_DESC;

		struct common_bind
		{};

		static constexpr auto DescriptorHeapType { ::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER };
		static constexpr auto RangeType          { ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER };
	};
	
	template<>
	struct Views::Bindable<SamplerView>
		: inner::DeviceChild 
	{
		Bindable(Device& device) 
			: DeviceChild{device}
		{}

		::D3D12_SAMPLER_DESC View;
	};

	template<::D3D12_COMMAND_LIST_TYPE type> 
		requires (type != ::D3D12_COMMAND_LIST_TYPE_COPY)
	struct inner::Operation<SamplerView, type> 
	{
		TWEN_ISCA CreateView{ &::ID3D12Device::CreateSampler };

		inline void operator()(::std::shared_ptr<Views::Bindable<SamplerView>> view, ::D3D12_CPU_DESCRIPTOR_HANDLE handle) const
		{
			view->GetDevicePointer()->CreateSampler(&view->View, handle);
		}
	};

}

//


//  BUFFER ==-----------------------------------------0


//


TWEN_EXPORT namespace twen
{

	template<typename View>
	struct Writable;

	template<typename Bindable>
	struct write_result;

	template<typename T>
	struct CheckFormatIsSupport 
	{
		TWEN_ISCA value{ ::std::integral<T> && (sizeof(T) <= 4) };
		TWEN_ISCA format
		{ 
			  ::std::is_same_v<T, ::UINT8>  ? ::DXGI_FORMAT_R8_UINT
			: ::std::is_same_v<T, ::UINT16> ? ::DXGI_FORMAT_R16_UINT
			: ::std::is_same_v<T, ::UINT32> ? ::DXGI_FORMAT_R32_UINT
			: ::DXGI_FORMAT_UNKNOWN 
		};
		using type = 
			::std::conditional_t<::std::is_same_v<T, ::UINT8>, ::UINT8,
			::std::conditional_t<::std::is_same_v<T, ::UINT16>, ::UINT16,
			::std::conditional_t<::std::is_same_v<T, ::UINT32>, ::UINT32,
			void>>>;
	};

	template<typename View>
		requires requires{ View::always_upload; }
	struct Writable<View> 
	{
	private:
		using self_t = Views::Bindable<View, Writable<View>>;
	public:
		~Writable() 
		{
			if (m_Mapped.size()) 
			{
				auto bindable = static_cast<self_t*>(this);
				auto resource = bindable->BackingResource();
				auto pointer  = bindable->ResourcePosition();
				resource->UnMap(m_Mapped);
			}
		}

		template<typename Fn>
		void Write(Fn&& fn) requires
			(::std::invocable<Fn, ::std::span<::std::byte> >
				&& ::std::is_same_v<::std::invoke_result_t<Fn, ::std::span<::std::byte>>, typename write_result<self_t>::type >)
		{
			using result_t = ::std::invoke_result_t<Fn, ::std::span<::std::byte>>;
			// Actual type of this.
			self_t* bindable = static_cast<self_t*>(this);
			// map resource if havent be mapped.
			if (m_Mapped.empty())
			{
				auto resource = bindable->BackingResource();
				auto pointer = bindable->ResourcePosition();
				m_Mapped = resource->Map(pointer.Offset, pointer.Size);
			}
			// staging buffer 
			if constexpr (!::std::is_void_v<result_t>)
			{
				auto writeRes{ fn(m_Mapped) };
				if constexpr (Views::is_vbv<View>)
				{
					for (auto accumlate{ 0u }; auto const& result : writeRes)
					{
						auto const& view{ bindable->View().emplace_back(bindable->Address() + accumlate, static_cast<::UINT>(result.Size), static_cast<::UINT>(result.Stride)) };
					#if D3D12_MODEL_DEBUG
						bindable->BackingResource()->VerifyResourceLocation(view.BufferLocation, m_Mapped.subspan(accumlate, view.SizeInBytes));
					#endif
						accumlate += static_cast<::UINT>(result.Size);
					}

				} else if constexpr (Views::is_ibv<View>) {
					bindable->View() = { bindable->Address(), writeRes.Size, writeRes.Format };
				#if D3D12_MODEL_DEBUG
					bindable->BackingResource()->VerifyResourceLocation(bindable->View().BufferLocation, m_Mapped.subspan(0u, bindable->View().SizeInBytes));
				#endif
				} else {
					bindable->View() = { bindable->Address(), writeRes };
				#if D3D12_MODEL_DEBUG
					bindable->BackingResource()->VerifyResourceLocation(bindable->View().BufferLocation, m_Mapped.subspan(0u, bindable->View().SizeInBytes));
				#endif
				}
			}
			else fn(m_Mapped);
		}

	protected:

		// Not allow construct directly.
		constexpr Writable() = default;

	private:

		::std::span<::std::byte> m_Mapped{};
	};






	struct VertexBufferView
	{
		using view_t = ::std::vector<::D3D12_VERTEX_BUFFER_VIEW>;

		TWEN_ISCA ViewType{ Views::VBV };

		TWEN_ISCA always_upload{ true };

		TWEN_ISCA HeapType{ ::D3D12_HEAP_TYPE_UPLOAD };
	};

	using VertexBuffer = Views::Bindable<VertexBufferView, Writable<VertexBufferView>>;





	struct IndexBufferView
	{
		using view_t = ::D3D12_INDEX_BUFFER_VIEW;

		TWEN_ISCA ViewType{ Views::IBV };

		TWEN_ISCA always_upload{ true };
	};

	using IndexBuffer = Views::Bindable<IndexBufferView, Writable<IndexBufferView>>;








	struct ConstantBufferView
	{
		using view_t = ::D3D12_CONSTANT_BUFFER_VIEW_DESC;

		struct common_bind {};

		TWEN_ISCA ViewType{ Views::CSV };
		TWEN_ISCA always_upload{ true };

		TWEN_ISCA RootType{ ::D3D12_ROOT_PARAMETER_TYPE_CBV };
		TWEN_ISCA RangeType{ ::D3D12_DESCRIPTOR_RANGE_TYPE_CBV };

		TWEN_ISCA DescriptorHeapType{ ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
	};

	using ConstantBuffer = Views::Bindable<ConstantBufferView, Writable<ConstantBufferView>>;

	template<::D3D12_COMMAND_LIST_TYPE type>
	struct inner::Operation<ConstantBufferView, type>
	{
		TWEN_ISCA Bind
		{
			type == ::D3D12_COMMAND_LIST_TYPE_DIRECT
			? &::ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView
			: &::ID3D12GraphicsCommandList::SetComputeRootConstantBufferView
		};
		TWEN_ISCA CreateView{ &::ID3D12Device::CreateConstantBufferView };

		void operator()(::std::shared_ptr<ConstantBuffer> cb, ::D3D12_CPU_DESCRIPTOR_HANDLE handle) const
		{
			cb->GetDevicePointer()->CreateConstantBufferView(&cb->View(), handle);
		}

		void operator()(::std::shared_ptr<ConstantBuffer> cb, ::ID3D12GraphicsCommandList* cmdlist, ::UINT root) const
		{
			(cmdlist->*Bind)(root, cb->View().BufferLocation);
		}
	};

	template<typename View> requires
		requires{ View::always_upload; }
	struct inner::ResourceCreationInfo<View> : inner::ResourceCreationInfo<void>
	{
		TWEN_ISCA HeapType{ ::D3D12_HEAP_TYPE_UPLOAD };
	};




	struct StagingBufferView
	{
		// place holder type that allow create view.
		struct view_t{};

		TWEN_ISCA ViewType{ Views::Staging };
		TWEN_ISCA always_upload{ false };
	};

	using StagingBuffer = Views::Bindable<StagingBufferView, Writable<StagingBufferView>>;





	template<>
	struct write_result<StagingBuffer>
	{
		using type = void;
	};

	template<>
	struct write_result<VertexBuffer>
	{
		struct param_type
		{
			::UINT64 Stride;
			::UINT64 Size;
		};

		using type = ::std::vector<param_type>;
	};

	template<>
	struct write_result<ConstantBuffer>
	{
		using type = ::UINT;
	};

	template<>
	struct write_result<IndexBuffer>
	{
		struct type
		{
			::DXGI_FORMAT Format;
			::UINT Size;
		};
	};

	template<typename BindableView>
	using WriteResult = typename write_result<BindableView>::type;

}

namespace twen
{

}

