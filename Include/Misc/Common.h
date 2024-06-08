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
#elif !defined(WINRT_BASE_H) && defined(_WRL_IMPLEMENTS_H_)
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

	namespace inner
	{
		class DeviceChild
		{
		public:
			inline DeviceChild(::twen::Device& device) : m_Device{ &device } {}

			inline auto& GetDevice() const
			{
				MODEL_ASSERT(m_Device, "Device cannot be null.");
				return *m_Device;
			}
			inline::ID3D12Device* GetDevicePointer() const;

			inline bool IsSameDevice(DeviceChild const& other) const { return m_Device == other.m_Device; }
			inline bool IsSameAdapter(DeviceChild const& other) const;
		private:
			::twen::Device* m_Device;
		};
	}
}

// Node mask.
namespace twen::inner
{
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
			: VisibleMask{ nodeMask.Visible | nodeMask.Creation }, CreateMask{ nodeMask.Creation } {}
		MultiNodeObject(::UINT visibleMask, ::UINT createMask)
			: VisibleMask{ visibleMask | createMask }, CreateMask{ createMask } {}

		// temporary.
		bool IntersectWith(MultiNodeObject const&) const { return false; }

		const::UINT VisibleMask;
		const::UINT CreateMask;
	};
}

namespace twen::Views 
{
	// Indicate resource view's type.
	// This is use for dyanmic creation.
	enum Type 
	{
		// shader resource view
		SRV, 
		// unordered access view
		UAV, 
		// Render target view
		RTV, 
		// Depth stencil view
		DSV, 
		// Sampler
		Sampler, 
		// Indirect argument.
		Indirect, 
		// Constant buffer view
		CSV, 
		// Vertex buffer view
		VBV,
		// Index buffer view.
		IBV,
		// Staging buffer view
		Staging
	};


}

namespace twen::Constants 
{
	TWEN_ISCA MaximumSmallResourceSize{ 64 * 1024u };

	TWEN_ISCA SmallResourceAlignment{ 4 * 1024u };

	TWEN_ISC::UINT64 NormalAlignment{ D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT };
	TWEN_ISC::UINT64 NormalMsaaAlignment{ D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT };

	TWEN_ISC float ClearValue[4] { 0.5f, 0.5f, 0.5f, 1.0f };
	TWEN_ISC float DepthClear    {1.0f};
	TWEN_ISC::UINT8 StencilClear {0xff};
}

namespace twen::Utils
{
	TWEN_IC::UINT GetBitPerPixel(::DXGI_FORMAT format)
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
	TWEN_IC::DXGI_FORMAT GetDepthFormat(::DXGI_FORMAT format)
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
	inline constexpr::DXGI_FORMAT GetDSVFormat(::DXGI_FORMAT format)
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
	inline constexpr::DXGI_FORMAT GetUAVFormat(::DXGI_FORMAT format)
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
	inline constexpr::DXGI_FORMAT GetTypelessFormat(::DXGI_FORMAT format)
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

	inline constexpr::UINT GetSubresourceID(UINT mipSlice, UINT arraySlice, UINT planeSlice, UINT mipLevels, UINT arraySize)
	{
		return mipSlice + arraySlice * planeSlice + mipLevels * planeSlice * arraySize;
	}
	// For uncompressed resource and 2d resource desc is ok.
	// 3D resource might have plane slice ?
	inline constexpr auto SubResourceCount(::D3D12_RESOURCE_DESC const& desc) noexcept
	{

		return ((::UINT)desc.MipLevels * desc.DepthOrArraySize);
	}
	inline::UINT16 GetMipLevel(::UINT64 width, ::UINT height)
	{
		return static_cast<UINT16>(::std::floor(::std::log2(width > height ? width : height))) + 1;
	}

	inline::DXGI_FORMAT MakeFormat(::D3D_REGISTER_COMPONENT_TYPE type, ::UINT mask) noexcept
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
	inline::UINT FormatToMask(::DXGI_FORMAT format)
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

	inline constexpr::D3D12_ROOT_PARAMETER_TYPE ToRootParameterType(::D3D_SHADER_INPUT_TYPE type) noexcept
	{
		switch (type)
		{
		case D3D_SIT_CBUFFER:
			return::D3D12_ROOT_PARAMETER_TYPE_CBV;

		case D3D_SIT_TBUFFER:
		case D3D_SIT_BYTEADDRESS:
		case D3D_SIT_STRUCTURED:
			return::D3D12_ROOT_PARAMETER_TYPE_SRV;

		case D3D_SIT_UAV_RWTYPED:
		case D3D_SIT_UAV_RWSTRUCTURED:
		case D3D_SIT_UAV_RWBYTEADDRESS:
		case D3D_SIT_UAV_APPEND_STRUCTURED:
		case D3D_SIT_UAV_CONSUME_STRUCTURED:
			return::D3D12_ROOT_PARAMETER_TYPE_UAV;

		default:return::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		}
	}

	inline constexpr::D3D12_RESOURCE_DIMENSION GetDimensionBySRVDimension(::D3D_SRV_DIMENSION dimension)
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

	inline constexpr bool IsFormatCompressed(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return true;
		default:
			return false;
		}
	}

	inline constexpr::D3D12_DESCRIPTOR_RANGE_TYPE ToDescriptorRangeType(::D3D_SHADER_INPUT_TYPE type) noexcept
	{
		switch (type)
		{
		case D3D_SIT_CBUFFER:
			return::D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

		case D3D_SIT_TBUFFER:
		case D3D_SIT_STRUCTURED:
		case D3D_SIT_BYTEADDRESS:
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

	inline constexpr::D3D12_DESCRIPTOR_RANGE_TYPE RootTypeToRangeType(::D3D12_ROOT_PARAMETER_TYPE type)
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

	inline constexpr::D3D12_SHADER_BYTECODE(
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


	inline constexpr auto IsSmallResource(::D3D12_RESOURCE_DESC const& desc) noexcept
	{
		if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
			return false;
		if (!(desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)))
			return false;
		if (desc.SampleDesc.Count > 1)
			return false;
		if (desc.DepthOrArraySize != 1)
			return false;

		UINT64 sizeX = desc.Width;
		UINT sizeY = desc.Height;
		UINT bitsPerPixel = GetBitPerPixel(desc.Format);
		if (bitsPerPixel == 0)
			return false;

		if (IsFormatCompressed(desc.Format))
		{
			sizeX = (sizeX + 3) >> 2;
			sizeY = (sizeY + 3) >> 2;
			bitsPerPixel *= 16;
		}

		UINT tileSizeX = 0, tileSizeY = 0;
		switch (bitsPerPixel)
		{
		case   8: tileSizeX = 64; tileSizeY = 64; break;
		case  16: tileSizeX = 64; tileSizeY = 32; break;
		case  32: tileSizeX = 32; tileSizeY = 32; break;
		case  64: tileSizeX = 32; tileSizeY = 16; break;
		case 128: tileSizeX = 16; tileSizeY = 16; break;
		default: return false;
		}

		const UINT64 tileCount = ((sizeX + tileSizeX - 1) / tileSizeX) * ((sizeY + tileSizeY - 1) / tileSizeY);
		return tileCount <= 16;
	}

	inline constexpr auto ResourceBarrier(::ID3D12Resource* resource,
		::D3D12_RESOURCE_STATES stateBefore, ::D3D12_RESOURCE_STATES stateAfter,
		::UINT subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		::D3D12_RESOURCE_BARRIER_FLAGS flags = ::D3D12_RESOURCE_BARRIER_FLAG_NONE)
	{
		return::D3D12_RESOURCE_BARRIER{ ::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, flags, { resource, subresourceIndex, stateBefore, stateAfter } };
	}

	inline constexpr auto ResourceBarrier(::ID3D12Resource* before, ::ID3D12Resource* after,
		::D3D12_RESOURCE_BARRIER_FLAGS flags = ::D3D12_RESOURCE_BARRIER_FLAG_NONE)
	{
		return::D3D12_RESOURCE_BARRIER{ ::D3D12_RESOURCE_BARRIER_TYPE_ALIASING, flags, {.Aliasing{before, after} } };
	}

	inline constexpr::D3D12_RESOURCE_BARRIER ResourceBarrier(::ID3D12Resource* uav,
		::D3D12_RESOURCE_BARRIER_FLAGS flags = ::D3D12_RESOURCE_BARRIER_FLAG_NONE)
	{
		return::D3D12_RESOURCE_BARRIER{ ::D3D12_RESOURCE_BARRIER_TYPE_UAV, flags, {.UAV{uav}} };
	}

	inline::UINT64 Align2(::UINT64 size, ::UINT64 align, bool up)
	{
		MODEL_ASSERT(align || (align & (align - 1)), "Align must be power of 2.");
		return (size + up * (align - 1u)) & ~(align - 1u);
	}

	inline constexpr::UINT64 Align(::UINT64 size, ::UINT64 align, bool up)
	{
		return ((size + up * (align - 1u)) / align) * align;
	}

	// Actual size. included mipmap size.
	inline constexpr::UINT64 DescSizeNoAligned(::D3D12_RESOURCE_DESC const& desc) 
	{
		if (desc.Dimension == ::D3D12_RESOURCE_DIMENSION_BUFFER) return desc.Width;

		assert(desc.Format != ::DXGI_FORMAT_UNKNOWN && "Texture not allow unknown format.");
		// single texture size.
		auto singleSize =
			desc.Width * (desc.Format != ::DXGI_FORMAT_UNKNOWN ? Utils::GetBitPerPixel(desc.Format) >> 3 : 1)
			* desc.Height
			* desc.DepthOrArraySize;
		return (singleSize + singleSize / 3) * desc.SampleDesc.Count;
	}
	// Conservative size.
	inline constexpr::UINT64 DescSize(::D3D12_RESOURCE_DESC const& desc) 
	{
		return Utils::Align(DescSizeNoAligned(desc), desc.Alignment, true);
	}

	inline constexpr auto TextureCopy(::ID3D12Resource* resource, ::UINT index) 
	{
		return::D3D12_TEXTURE_COPY_LOCATION
		{
			.pResource{resource},
			.Type{::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX},
			.SubresourceIndex{index},
		};
	}
	inline constexpr auto TextureCopy(::ID3D12Resource* resource, ::D3D12_PLACED_SUBRESOURCE_FOOTPRINT const& footprint) 
	{
		return::D3D12_TEXTURE_COPY_LOCATION
		{
			.pResource{resource},
			.Type{::D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT},
			.PlacedFootprint{footprint},
		};
	}
}

namespace twen::inner
{
	template<typename R>
	struct Allocator;

	template<typename T>
	struct Pointer;

	template<typename R>
	struct PointerBase 
	{
		::std::weak_ptr<R> Backing;

		::UINT64 Offset;
		::UINT64 Size;

		Allocator<R>* AllocateFrom{nullptr};

		void Fallback() const 
		{
			if (AllocateFrom)
				AllocateFrom->Free(static_cast<Pointer<R>>(*this));
			else if (StandAlone())
				Backing.lock()->Evict();
			else
				MODEL_ASSERT(false, "Neither the pointer was allocated nor represented an resource.");
		}

		bool StandAlone() const { return Backing.lock()->Size == Size; }
	};

	template<typename R>
	inline bool operator==(Pointer<R> const& left, Pointer<R> const& right) 
	{
		return left.Backing.lock().get() == right.Backing.lock().get()
			&& left.Offset == right.Offset && left.Size == right.Size;
	}
}

namespace twen::inner
{	
	template<typename Derived, typename Child>
	concept congener =
		::std::is_base_of_v<Child, Derived>
		|| ::std::is_same_v<Derived, Child>;

	template<typename T>
	struct ShareObject : public::std::enable_shared_from_this<T>
	{
		using share = ::std::enable_shared_from_this<T>;
	};
}
 

namespace twen
{
	enum ResourcePreference : ::UINT8
	{
		// - use default strategy to allocate resource.
		//   if the resource is an small resource, 
		//   then allocate in small committed resource.
		//   And allocate texture on heap.
		DefaultResource = 0x0,
		// - Indicate resource is not actually an small resource,
		//   although the order seems like an small resource 
		//   when it was be hanled by allocator.
		//   Use for resources that upload multi-time.
		//   Buffer will always 65536u, so no need to set this flags.
		DenySmallResource = 0x1,
		// - Indicate resource should not place on an heap.
		//   Common use on some small resource or staging buffer.
		DenyPlacedResource = 0x2,
		// - This is only for texture, use on buffer will be ignore.
		//   Indicate that the texture is tiled by an fix size and just allocate 
		//   a enough big pool that can directly allocate from it.
		FixStride = 0x4,
	};

	// Resource create request.
	struct ResourceOrder : public::D3D12_RESOURCE_DESC
	{
		// Buffer desc.
		ResourceOrder(::UINT64 size) noexcept
			: ::D3D12_RESOURCE_DESC
			{
			::D3D12_RESOURCE_DIMENSION_BUFFER
			, 65536u
			, size
			, 1u
			, 1u
			, 1u
			, ::DXGI_FORMAT_UNKNOWN
			, { 1u, 0u }
			, ::D3D12_TEXTURE_LAYOUT_ROW_MAJOR
			}
		{}

		// Texture2D desc.
		ResourceOrder(::UINT64 width, ::UINT height, ::DXGI_FORMAT format, ::UINT16 arraySize = 1u, 
			ResourcePreference preference = ResourcePreference::DefaultResource) noexcept
			: ::D3D12_RESOURCE_DESC
			{
			::D3D12_RESOURCE_DIMENSION_TEXTURE2D
			, 0u
			, width
			, height
			, arraySize
			, Utils::GetMipLevel(width, height)
			, format
			, {1u, 0u}
			, ::D3D12_TEXTURE_LAYOUT_UNKNOWN
			}
		{
			if (!(preference & ResourcePreference::DenySmallResource) && Utils::IsSmallResource(*this))
				Alignment = Constants::SmallResourceAlignment;
			else
				Alignment = Constants::NormalAlignment;
		}

		// Make Texutre1D desc.
		ResourceOrder(::UINT64 width, ::DXGI_FORMAT format, ::UINT16 arraySize = 1u, 
			ResourcePreference preference = ResourcePreference::DefaultResource) noexcept
			: ::D3D12_RESOURCE_DESC
			{
			::D3D12_RESOURCE_DIMENSION_TEXTURE1D
			, 0u
			, width
			, 1u
			, arraySize
			, Utils::GetMipLevel(width, 1u)
			, format
			, { 1u, 0u }
			, ::D3D12_TEXTURE_LAYOUT_UNKNOWN
			}
		{
			if (!(preference & DenySmallResource) && Utils::IsSmallResource(*this))
				Alignment = Constants::SmallResourceAlignment;
			else
				Alignment = Constants::NormalAlignment;
		}

		// For multi sample texture2D. 
		ResourceOrder(::UINT64 width, ::UINT height, ::DXGI_SAMPLE_DESC const& sampleDesc,
			::DXGI_FORMAT format, ::UINT16 arraySize = 1u) noexcept
			: ::D3D12_RESOURCE_DESC
			{
			::D3D12_RESOURCE_DIMENSION_TEXTURE2D
			, sampleDesc.Count == 1u
			? Constants::NormalAlignment
			: Constants::NormalMsaaAlignment
			, width
			, height
			, arraySize
			, 1u
			, format
			, sampleDesc
			, ::D3D12_TEXTURE_LAYOUT_UNKNOWN
			}
		{}

		// Texture3D desc.
		ResourceOrder(::UINT64 width, ::UINT height, ::UINT16 depth,
			::DXGI_FORMAT format, 
			ResourcePreference preference = ResourcePreference::DefaultResource) noexcept
			: ::D3D12_RESOURCE_DESC
			{
			::D3D12_RESOURCE_DIMENSION_TEXTURE3D
			, Constants::NormalAlignment
			, width
			, height
			, depth
			, 1u
			, format
			, { 1u, 0u }
			, ::D3D12_TEXTURE_LAYOUT_UNKNOWN
			}
		{
			if (!(preference & DenySmallResource) && Utils::IsSmallResource(*this))
				Alignment = Constants::SmallResourceAlignment;
			else
				Alignment = Constants::NormalAlignment;
		}

		auto&& TextureLayout(::D3D12_TEXTURE_LAYOUT layout)
		{
			MODEL_ASSERT(Dimension != ::D3D12_RESOURCE_DIMENSION_BUFFER, "Buffer can only be row major.");

			Layout = layout;
			if (layout == ::D3D12_TEXTURE_LAYOUT_ROW_MAJOR)
				MipLevels = 1u;
			return *this;
		}
		auto&& SetFlags(::D3D12_RESOURCE_FLAGS extra)
		{
			::D3D12_RESOURCE_DESC::Flags = extra;
			return *this;
		}

		::UINT NodeMask : 16{};
		auto&& SetNodeMask(::UINT node)
		{
			NodeMask = node;
			return *this;
		}

		::D3D12_HEAP_FLAGS HeapFlags : 16{};
		auto&& SetHeapFlags(::D3D12_HEAP_FLAGS flags)
		{
			HeapFlags = flags;
			return *this;
		}

		::D3D12_HEAP_TYPE HeapType : 8{};
		auto&& SetHeapType(::D3D12_HEAP_TYPE type)
		{
			HeapType = type;
			return *this;
		}
		auto&& MarkUpload()
		{
			MODEL_ASSERT(!Height, "Texture would not allow upload.");

			HeapType = ::D3D12_HEAP_TYPE_UPLOAD;
			return *this;
		}
		auto&& MarkReadback()
		{
			MODEL_ASSERT(!Height, "Texture would not allow readback.");

			HeapType = ::D3D12_HEAP_TYPE_READBACK;
			return *this;
		}

		//::D3D12_CLEAR_VALUE ClearValue{};
		//auto&& SetClearValue(DXGI_FORMAT format, float r, float g, float b, float a = 1.0f) 
		//{
		//	ClearValue = { format, r, g, b, a };
		//	return *this;
		//}
		//auto&& SetClearValue(::DXGI_FORMAT format, float depth, ::UINT8 mask = 0xff) 
		//{
		//	ClearValue = { .Format{format}, .DepthStencil{ depth, mask } };
		//	return *this;
		//}
	};
}

namespace twen 
{
	// Represent node.
	template<typename FrontT, typename BackT>
	struct LinkedObject 
	{
		using front_t = FrontT;
		using back_t = BackT;

		FrontT* Front;
		BackT* Back;
	};

	// Represent tail.
	template<typename FrontT>
	struct LinkedObject<FrontT, void>
	{
		using front_t = FrontT;

		FrontT* Front;
	};

	// Represent head.
	template<typename BackT>
	struct LinkedObject<void, BackT>
	{
		using back_t = BackT;

		BackT* Back;
	};
}

namespace twen::Debug 
{
	::std::wstring_view Name();
	void Name(::std::wstring_view name);
}