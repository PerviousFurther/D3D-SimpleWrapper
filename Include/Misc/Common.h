#pragma once

namespace twen
{
	template<typename T> struct ComPtr
#if defined(WINRT_BASE_H)
		: ::winrt::com_ptr<T>
	{
		using::winrt::com_ptr<T>::com_ptr;
		ComPtr(::winrt::com_ptr<T> p) : ::winrt::com_ptr<T>{ p } {}
		inline T* Get() const { return::winrt::com_ptr<T>::get(); }
		inline T* operator*() const { return::winrt::com_ptr<T>::get(); }
		inline operator T* const() const { return::winrt::com_ptr<T>::get(); }

		inline T** Put() { return::winrt::com_ptr<T>::put(); }
		inline T** operator&() { return::winrt::com_ptr<T>::put(); }

		template<typename P>
		inline ComPtr<P> As() { return::winrt::com_ptr<T>::template try_as<P>(); }
	};
#elif defined(_WRL_IMPLEMENTS_H_)
		: ::Microsoft::WRL::ComPtr<T>
	{
		using::Microsoft::WRL::ComPtr<T>::ComPtr;
		ComPtr(::Microsoft::WRL::ComPtr<T> p) : ::Microsoft::WRL::ComPtr<T>{ p } {}

		inline T* Get() const { return::Microsoft::WRL::ComPtr<T>::Get(); }
		inline T* operator*() const { return Get(); }

		inline T** operator&() { return::Microsoft::WRL::ComPtr<T>::ReleaseAndGetAddressOf(); }
		inline T** Put() { return::Microsoft::WRL::ComPtr<T>::ReleaseAndGetAddressOf(); }

		template<typename P>
		inline ComPtr<P> As()
		{
			::Microsoft::WRL::ComPtr<P> pointer;
			auto hr = ::Microsoft::WRL::ComPtr<T>::As(&pointer);
			assert(SUCCEEDED(hr) && "ComPtr as failure.");
			return pointer;
		}
	};
#else
		;
#error Custom ComPtr is not implmented.
#endif
}

// Device child.
namespace twen
{
	class Device;

	class DeviceChild
	{
	public:
		FORCEINLINE DeviceChild(Device& device) : m_Device{ &device } {}

		FORCEINLINE auto& GetDevice() const
		{
			assert(m_Device && "Device cannot be null.");
			return *m_Device;
		}
	private:
		Device* m_Device;
	};
}

// Node mask.
namespace twen
{
	class Device;

	struct NodeMask
	{
		::UINT Creation;
		::UINT Visible;
	};

	struct SingleNodeObject
	{
		SingleNodeObject(NodeMask node) : NativeMask{ node.Creation } {}
		SingleNodeObject(::UINT mask)
			: NativeMask{ mask } {}

		bool InteractableWith(SingleNodeObject const&) const { return false; }
		const::UINT NativeMask;
	};

	struct MultiNodeObject
	{
		MultiNodeObject(NodeMask nodeMask)
			: VisibleMask{ nodeMask.Visible }, CreateMask{ nodeMask.Creation } {}
		MultiNodeObject(::UINT visibleMask, ::UINT createMask)
			: VisibleMask{ visibleMask }, CreateMask{ createMask } {}

		// temporary.
		bool IntersectWith(MultiNodeObject const&) const { return false; }

		const::UINT VisibleMask;
		const::UINT CreateMask;
	};
}

namespace twen
{
	inline constexpr struct
	{
		inline static constexpr::UINT GetBitPerPixel(::DXGI_FORMAT format)
		{
			// From dxtk12
			//-------------------------------------------------------------------------------------
			// Returns bits-per-pixel for a given DXGI format, or 0 on failure
			//-------------------------------------------------------------------------------------
			switch (format)
			{
			case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
			case DXGI_FORMAT_R32G32B32A32_UINT:
			case DXGI_FORMAT_R32G32B32A32_SINT:
				return 128;

			case DXGI_FORMAT_R32G32B32_TYPELESS:
			case DXGI_FORMAT_R32G32B32_FLOAT:
			case DXGI_FORMAT_R32G32B32_UINT:
			case DXGI_FORMAT_R32G32B32_SINT:
				return 96;

			case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_UNORM:
			case DXGI_FORMAT_R16G16B16A16_UINT:
			case DXGI_FORMAT_R16G16B16A16_SNORM:
			case DXGI_FORMAT_R16G16B16A16_SINT:
			case DXGI_FORMAT_R32G32_TYPELESS:
			case DXGI_FORMAT_R32G32_FLOAT:
			case DXGI_FORMAT_R32G32_UINT:
			case DXGI_FORMAT_R32G32_SINT:
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			case DXGI_FORMAT_Y416:
			case DXGI_FORMAT_Y210:
			case DXGI_FORMAT_Y216:
				return 64;

			case DXGI_FORMAT_R10G10B10A2_TYPELESS:
			case DXGI_FORMAT_R10G10B10A2_UNORM:
			case DXGI_FORMAT_R10G10B10A2_UINT:
			case DXGI_FORMAT_R11G11B10_FLOAT:
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_UINT:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_R8G8B8A8_SINT:
			case DXGI_FORMAT_R16G16_TYPELESS:
			case DXGI_FORMAT_R16G16_FLOAT:
			case DXGI_FORMAT_R16G16_UNORM:
			case DXGI_FORMAT_R16G16_UINT:
			case DXGI_FORMAT_R16G16_SNORM:
			case DXGI_FORMAT_R16G16_SINT:
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_UINT:
			case DXGI_FORMAT_R32_SINT:
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			case DXGI_FORMAT_R8G8_B8G8_UNORM:
			case DXGI_FORMAT_G8R8_G8B8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			case DXGI_FORMAT_AYUV:
			case DXGI_FORMAT_Y410:
			case DXGI_FORMAT_YUY2:
				//case XBOX_DXGI_FORMAT_R10G10B10_7E3_A2_FLOAT:
				//case XBOX_DXGI_FORMAT_R10G10B10_6E4_A2_FLOAT:
				//case XBOX_DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM:
				return 32;

			case DXGI_FORMAT_P010:
			case DXGI_FORMAT_P016:
				//case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
				//case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
				//case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:
				//case WIN10_DXGI_FORMAT_V408:
				return 24;

			case DXGI_FORMAT_R8G8_TYPELESS:
			case DXGI_FORMAT_R8G8_UNORM:
			case DXGI_FORMAT_R8G8_UINT:
			case DXGI_FORMAT_R8G8_SNORM:
			case DXGI_FORMAT_R8G8_SINT:
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UNORM:
			case DXGI_FORMAT_R16_UINT:
			case DXGI_FORMAT_R16_SNORM:
			case DXGI_FORMAT_R16_SINT:
			case DXGI_FORMAT_B5G6R5_UNORM:
			case DXGI_FORMAT_B5G5R5A1_UNORM:
			case DXGI_FORMAT_A8P8:
			case DXGI_FORMAT_B4G4R4A4_UNORM:
				//case WIN10_DXGI_FORMAT_P208:
				//case WIN10_DXGI_FORMAT_V208:
				return 16;

			case DXGI_FORMAT_NV12:
			case DXGI_FORMAT_420_OPAQUE:
			case DXGI_FORMAT_NV11:
				return 12;

			case DXGI_FORMAT_R8_TYPELESS:
			case DXGI_FORMAT_R8_UNORM:
			case DXGI_FORMAT_R8_UINT:
			case DXGI_FORMAT_R8_SNORM:
			case DXGI_FORMAT_R8_SINT:
			case DXGI_FORMAT_A8_UNORM:
			case DXGI_FORMAT_AI44:
			case DXGI_FORMAT_IA44:
			case DXGI_FORMAT_P8:
				//case XBOX_DXGI_FORMAT_R4G4_UNORM:
				return 8;

			case DXGI_FORMAT_R1_UNORM:
				return 1;

			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
			case DXGI_FORMAT_BC4_SNORM:
				return 4;

			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
			case DXGI_FORMAT_BC5_SNORM:
			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
			case DXGI_FORMAT_BC6H_SF16:
			case DXGI_FORMAT_BC7_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				return 8;

			default:
				return 0;
			}
		}
		inline static constexpr::DXGI_FORMAT GetDepthFormat(::DXGI_FORMAT format)
		{
			// Copy from DirectX-Graphics-Sample.
			switch (format)
			{
				// 32-bit Z w/ Stencil
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
				return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

				// 24-bit Z
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
				return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

			default:
				return DXGI_FORMAT_UNKNOWN;
			}
		}
		inline static constexpr::DXGI_FORMAT GetDSVFormat(::DXGI_FORMAT format)
		{
			// Copy from DirectX-Graphics-Sample.
			switch (format)
			{
				// 32-bit Z w/ Stencil
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
				return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

				// No Stencil
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
				return DXGI_FORMAT_D32_FLOAT;

				// 24-bit Z
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
				return DXGI_FORMAT_D24_UNORM_S8_UINT;

				// 16-bit Z w/o Stencil
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UNORM:
				return DXGI_FORMAT_D16_UNORM;

			default:
				return DXGI_FORMAT_UNKNOWN;
			}
		}
		inline static constexpr::DXGI_FORMAT GetUAVFormat(::DXGI_FORMAT format)
		{
			// Copy from DirectX-Graphics-Sample.
			switch (format)
			{
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				return DXGI_FORMAT_R8G8B8A8_UNORM;

			case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
				return DXGI_FORMAT_B8G8R8A8_UNORM;

			case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
				return DXGI_FORMAT_B8G8R8X8_UNORM;

			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_R32_FLOAT:
				return DXGI_FORMAT_R32_FLOAT;

#if IS_DEBUG
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			case DXGI_FORMAT_D16_UNORM:
				assert(!"Requested a UAV format for a depth stencil format.");

#endif
			default:
				// return format.
				return DXGI_FORMAT_UNKNOWN;
			}
		}
		inline static constexpr::DXGI_FORMAT GetTypelessFormat(::DXGI_FORMAT format)
		{
			// Copy from DirectX-Graphics-Sample.
			switch (format)
			{
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				return DXGI_FORMAT_R8G8B8A8_TYPELESS;

			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
				return DXGI_FORMAT_B8G8R8A8_TYPELESS;

			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
				return DXGI_FORMAT_B8G8R8X8_TYPELESS;

				// 32-bit Z w/ Stencil
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
				return DXGI_FORMAT_R32G8X24_TYPELESS;

				// No Stencil
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
				return DXGI_FORMAT_R32_TYPELESS;

				// 24-bit Z
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
				return DXGI_FORMAT_R24G8_TYPELESS;

				// 16-bit Z w/o Stencil
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UNORM:
				return DXGI_FORMAT_R16_TYPELESS;

			default:
				return format;
			}
		}

		FORCEINLINE static constexpr::UINT GetSubresourceID(UINT mipSlice, UINT arraySlice, UINT planeSlice, UINT mipLevels, UINT arraySize)
		{
			return mipSlice + arraySlice * planeSlice + mipLevels * planeSlice * arraySize;
		}
		FORCEINLINE static::UINT16 GetMipLevel(::UINT width, ::UINT height)
		{
			return static_cast<UINT16>(::std::floor(::std::log2(width > height ? width : height))) + 1;
		}


		inline static::DXGI_FORMAT MakeFormat(::D3D_REGISTER_COMPONENT_TYPE type, ::UINT mask) noexcept
		{
#define BUNDLE(type)								\
switch (mask)										\
{													\
case 0x1:return::DXGI_FORMAT_R32_##type;			\
case 0x3:return::DXGI_FORMAT_R32G32_##type;			\
case 0x7:return::DXGI_FORMAT_R32G32B32_##type;		\
case 0xf:return::DXGI_FORMAT_R32G32B32A32_##type;	\
default:return::DXGI_FORMAT_UNKNOWN;				\
}

			switch (type)
			{
			case::D3D_REGISTER_COMPONENT_UINT32:
				BUNDLE(UINT);
			case::D3D_REGISTER_COMPONENT_SINT32:
				BUNDLE(SINT);
			case::D3D_REGISTER_COMPONENT_FLOAT32:
				BUNDLE(FLOAT);
			default:return::DXGI_FORMAT_UNKNOWN;
			}
#undef BUNDLE
		}
		inline static::UINT FormatToMask(::DXGI_FORMAT format)
		{
#define BUNDLE(type)								\
case::DXGI_FORMAT_R32_##type:			return 0x1; \
case::DXGI_FORMAT_R32G32_##type:		return 0x3;	\
case::DXGI_FORMAT_R32G32B32_##type:		return 0x7; \
case::DXGI_FORMAT_R32G32B32A32_##type:	return 0xf; 
			switch (format) {
				BUNDLE(UINT); BUNDLE(FLOAT); BUNDLE(SINT);
			default:assert(!"Current format is not allow to make mask."); return 0;
			}
#undef BUNDLE
		}

		inline static constexpr::D3D12_ROOT_PARAMETER_TYPE ShaderInputTypeToRootParameterType(::D3D_SHADER_INPUT_TYPE type) noexcept
		{
			switch (type)
			{
			case D3D_SIT_CBUFFER:
			case D3D_SIT_STRUCTURED:// TODO: make sure it is right.
			case D3D_SIT_BYTEADDRESS:
				return::D3D12_ROOT_PARAMETER_TYPE_CBV;

			case D3D_SIT_TBUFFER:
				return::D3D12_ROOT_PARAMETER_TYPE_SRV;

			case D3D_SIT_UAV_RWTYPED:
			case D3D_SIT_UAV_RWSTRUCTURED:
			case D3D_SIT_UAV_RWBYTEADDRESS:
			case D3D_SIT_UAV_APPEND_STRUCTURED:
			case D3D_SIT_UAV_CONSUME_STRUCTURED:
			case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
				return::D3D12_ROOT_PARAMETER_TYPE_UAV;

			default:return::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			}
		}

		inline static constexpr::D3D12_RESOURCE_DIMENSION GetDimensionBySRVDimension(::D3D_SRV_DIMENSION dimension)
		{
			switch (dimension)
			{
			case::D3D12_RESOURCE_DIMENSION_BUFFER:
				return::D3D12_RESOURCE_DIMENSION_BUFFER;

			case::D3D12_SRV_DIMENSION_TEXTURE1D:
			case::D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
				return::D3D12_RESOURCE_DIMENSION_TEXTURE1D;

			case::D3D12_SRV_DIMENSION_TEXTURE2D:
			case::D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
			case::D3D12_SRV_DIMENSION_TEXTURE2DMS:
			case::D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
			case::D3D12_SRV_DIMENSION_TEXTURECUBE:
			case::D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
				return::D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			case::D3D12_SRV_DIMENSION_TEXTURE3D:
				return::D3D12_RESOURCE_DIMENSION_TEXTURE3D;

			case::D3D_SRV_DIMENSION_UNKNOWN:
			default:return::D3D12_RESOURCE_DIMENSION_UNKNOWN;
			}
		}

		inline static constexpr::D3D12_DESCRIPTOR_RANGE_TYPE ShaderInputTypeToDescriptorRangeType(::D3D_SHADER_INPUT_TYPE type) noexcept
		{
			switch (type)
			{
			case D3D_SIT_CBUFFER:
			case D3D_SIT_STRUCTURED:
			case D3D_SIT_BYTEADDRESS:
				return::D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

			case D3D_SIT_TBUFFER:
			case D3D_SIT_TEXTURE:
				return::D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

			case D3D_SIT_SAMPLER:
				return::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

			case D3D_SIT_UAV_RWTYPED:
			case D3D_SIT_UAV_RWSTRUCTURED:
			case D3D_SIT_UAV_RWBYTEADDRESS:
			case D3D_SIT_UAV_APPEND_STRUCTURED:
			case D3D_SIT_UAV_CONSUME_STRUCTURED:
			case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
				return::D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

			default:
				return static_cast<::D3D12_DESCRIPTOR_RANGE_TYPE>(0xff);
			}
		}

		inline static constexpr::D3D12_DESCRIPTOR_RANGE_TYPE RootTypeToRangeType(::D3D12_ROOT_PARAMETER_TYPE type)
		{
			switch (type)
			{
			case D3D12_ROOT_PARAMETER_TYPE_CBV:
				return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

			case D3D12_ROOT_PARAMETER_TYPE_SRV:
				return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

			case D3D12_ROOT_PARAMETER_TYPE_UAV:
				return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

			default:return::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			}
		}

		inline static constexpr::D3D12_SHADER_BYTECODE(
			::D3D12_GRAPHICS_PIPELINE_STATE_DESC::* ShaderTypeToBindPoint(::D3D12_SHADER_VERSION_TYPE type) noexcept)
		{
			switch (type)
			{
			case D3D12_SHVER_PIXEL_SHADER:		return&::D3D12_GRAPHICS_PIPELINE_STATE_DESC::PS;
			case D3D12_SHVER_VERTEX_SHADER:		return&::D3D12_GRAPHICS_PIPELINE_STATE_DESC::VS;
			case D3D12_SHVER_GEOMETRY_SHADER:	return&::D3D12_GRAPHICS_PIPELINE_STATE_DESC::GS;
			case D3D12_SHVER_HULL_SHADER:		return&::D3D12_GRAPHICS_PIPELINE_STATE_DESC::HS;
			case D3D12_SHVER_DOMAIN_SHADER:		return&::D3D12_GRAPHICS_PIPELINE_STATE_DESC::DS;
			default:return nullptr;
			}
		}
	} Utils{};

	template<typename T>
	struct Pointer;

	template<>
	struct Pointer<void> {};

	struct PointerHasher
	{
		template<typename T>
		inline::std::size_t operator()(Pointer<T> const& pointer) const
		{
			return::std::hash<::std::size_t>(pointer.Offset);
		}
	};
}

// Concepts
namespace twen::inner
{
	template<typename Derived, typename Base>
	concept congener =
		::std::is_base_of_v<Base, Derived>
		|| ::std::is_same_v<Derived, Base>;

	template<typename T>
	struct traits;
}

namespace twen
{
	template<typename T, typename...NT>
	struct ShareObject : public::std::enable_shared_from_this<T>
	{
		template<inner::congener<T> TargetT = T, typename...Args>
		static auto Create(NT...necessaryArgs, Args&&...args)
			noexcept(::std::is_nothrow_constructible_v<TargetT, NT..., Args...>)
			requires::std::constructible_from<TargetT, NT..., Args...>
		{
			return::std::make_shared<TargetT>(necessaryArgs..., ::std::forward<Args>(args)...);
		}

		template<inner::congener<T> TargetT>
		auto As();

		virtual ~ShareObject() = default;

		inline static::UINT Growing{ 0u };
		const::UINT ID{ Growing++ };

		using base_t = T;
	};
}

namespace twen
{
	struct LinkedObject
	{
		LinkedObject* Prev;
		LinkedObject* Next;
	};
}

// Remove in future probably.

//struct CriticalSection
//{
//	CriticalSection() { ::InitializeCriticalSectionAndSpinCount(&CS, 8u); }
//	~CriticalSection() { ::DeleteCriticalSection(&CS); }

//	CRITICAL_SECTION CS;
//};

//struct Event 
//{
//	Event() :Handle{ ::CreateEventExW(nullptr, nullptr, NULL, EVENT_ALL_ACCESS) } {}
//	~Event() { ::CloseHandle(Handle); }

//	void Set() { ::SetEvent(Handle); }
//	void Reset() { ::ResetEvent(Handle); }

//	void Wait(DWORD milisecond = INFINITE) const 
//	{
//		WaitForSingleObject(Handle, milisecond);
//	}
//	::HANDLE Handle;
//};
//template<typename>
//struct ScopeLock;

//template<>
//struct ScopeLock<void>
//{
//	ScopeLock(Event& event) 
//		: Event{ &event }
//		, Deletion{&ScopeLock::ResetEvent}
//	{ event.Set(); }

//	ScopeLock(CriticalSection& cs)
//		: CS{ &cs } 
//		, Deletion{ &ScopeLock::LeaveCS }
//	{ ::EnterCriticalSection(&cs.CS); }

//	~ScopeLock() { (this->*Deletion)(); }

//	void LeaveCS() { ::LeaveCriticalSection(&CS->CS); }
//	void ResetEvent() { Event->Reset(); }

//	void (ScopeLock::* Deletion)();
//	union 
//	{
//		CriticalSection* CS;
//		Event* Event;
//	};
//};