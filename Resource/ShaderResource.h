#pragma once

#include "Resource.h"
#include "Bindable.hpp"

namespace twen 
{
	inline namespace ShaderResources
	{
		inline constexpr struct
		{
			inline constexpr bool operator()(::D3D12_RESOURCE_DIMENSION res, ::D3D12_SRV_DIMENSION srv) const noexcept
			{
				switch (res)
				{
				case D3D12_RESOURCE_DIMENSION_BUFFER:
					switch (srv)
					{
					case D3D12_SRV_DIMENSION_BUFFER:
						return true;
					default:
						return false;
					}
				case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
					switch (srv)
					{
					case D3D12_SRV_DIMENSION_TEXTURE1D:
					case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
						return true;
					default:return false;
					}
				case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
					switch (srv)
					{
					case::D3D12_SRV_DIMENSION_TEXTURE2D:
					case::D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
					case::D3D12_SRV_DIMENSION_TEXTURE2DMS:
					case::D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
					case::D3D12_SRV_DIMENSION_TEXTURECUBE:
					case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
						return true;
					default:return false;
					}
				case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
					switch (srv)
					{
					case D3D12_SRV_DIMENSION_TEXTURE3D:
						return true;
					default:return false;
					}
				default:return false;
				}
			}
		} IsDimensionMatch{};
	}
}

namespace twen 
{	
	class Buffer;
	class Texture;
	template<typename T>
	class StructureBuffer;
	
	class ShaderResource : public ShareObject<ShaderResource>, public DeviceChild
	{
	public:
		using sort_t =::D3D12_SRV_DIMENSION;
		using view_t =::D3D12_SHADER_RESOURCE_VIEW_DESC;

		static constexpr auto DescriptorHeapType{ ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
		static constexpr auto RootType{ ::D3D12_ROOT_PARAMETER_TYPE_SRV };
		static constexpr auto RangeType{ ::D3D12_DESCRIPTOR_RANGE_TYPE_SRV };

		static constexpr auto GraphicsBindRoot{ &::ID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView };
		static constexpr auto ComputeBindRoot{ &::ID3D12GraphicsCommandList::SetComputeRootShaderResourceView };

		static constexpr auto CreateView{ &::ID3D12Device::CreateShaderResourceView };

		static constexpr auto Default4Component{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING };
	public:
		inline::D3D_SHADER_INPUT_TYPE ShaderInputType()
		{
			switch (Type)
			{
			case D3D12_SRV_DIMENSION_BUFFER:
				return::D3D_SIT_TBUFFER;

			case D3D12_SRV_DIMENSION_TEXTURE1D:
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
			case D3D12_SRV_DIMENSION_TEXTURE2D:
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
			case D3D12_SRV_DIMENSION_TEXTURE2DMS:
			case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
			case D3D12_SRV_DIMENSION_TEXTURE3D:
			case D3D12_SRV_DIMENSION_TEXTURECUBE:
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
				return::D3D_SIT_TEXTURE;

			case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
				return::D3D_SIT_RTACCELERATIONSTRUCTURE;
			default:
				return::D3D_SIT_CBUFFER;
			}
		}

		::D3D12_GPU_VIRTUAL_ADDRESS Address(view_t const&) const { return m_Resource->BaseAddress(); } // reserved.
		operator::ID3D12Resource* () const { return *m_Resource; }
	public:
		const sort_t Type;
	protected:
		template<typename...Args>
		ShaderResource(sort_t type, Device& device, Args&&...args) 
			requires::std::constructible_from<CommittedResource, Device&, Args...>
			: DeviceChild{device}
			, Type{ type } 
			, m_Resource{ device.Create<CommittedResource>(::std::forward<Args>(args)...) }
		{}
		template<typename...Args>
		ShaderResource(sort_t type, Device& device, Args&&...args)
			requires::std::constructible_from<PlacedResource, Device&, Args...>
			: DeviceChild{device}
			, Type{ type }
			, m_Resource{ device.Create<PlacedResource>(::std::forward<Args>(args)...) }
		{}
	protected:
		::std::shared_ptr<Resource> m_Resource;
	};
	//
	//
	// Gpu buffer.
	//
	//
	class DECLSPEC_EMPTY_BASES Buffer : 
		public Bindings::Bindable<ShaderResource>,
		public Bindings::TransitionPart<Texture, false>
	{
	public:
		static constexpr auto Dimension{ ::D3D12_SRV_DIMENSION_BUFFER };
		static constexpr auto MaxElementCount{ 16384u };
	public:
		Buffer(Device& device, ::UINT64 size, ::UINT alignment = 65536u);

		template<typename CT>
		void Copy(CT& cc, ::std::shared_ptr<AccessibleBuffer> buffer);
	public:
		inline void SetView(::D3D12_BUFFER_SRV const& buffer, 
			::DXGI_FORMAT format =::DXGI_FORMAT_UNKNOWN)
		{
			assert(MaxElementCount >= buffer.NumElements);
			m_View.Format = format;
			m_View.Buffer = buffer;
		}
		// Is buffer a typed buffer.
		bool Typed() const { return m_View.Format != DXGI_FORMAT_UNKNOWN; }
	};

	//
	//
	// Texture(2D/3D)[array/cube].
	//
	//
	class Texture : 
		public Bindings::Bindable<ShaderResource>,
		public Bindings::TransitionPart<Texture, true>
	{
		friend class TextureCopy;
	public:
		struct ViewDesc
		{
			::UINT MipLevels{ static_cast<::UINT>(-1) };
			::UINT MostDetailedMip{ 0u };
			::UINT PlaneSlice{ 0u };
		};
	public:
		Texture(Device& device, ::D3D12_RESOURCE_DESC const& desc, ::D3D12_SRV_DIMENSION dimension, 
			::UINT visbleNode = 0u, ::D3D12_HEAP_FLAGS flags =::D3D12_HEAP_FLAG_NONE)
			: Bindable{ dimension, device, ::D3D12_HEAP_TYPE_DEFAULT, desc, ::D3D12_RESOURCE_STATE_COMMON, visbleNode, flags }
			, Format{ desc.Format }
			, ResourceSize{ Resource::GetAllocationSize(desc) } 
		{
			m_View.Shader4ComponentMapping = Default4Component;

			assert(ShaderResources::IsDimensionMatch(desc.Dimension, dimension) && "Texture dimension mismatch.");
			m_View.ViewDimension = dimension;

			assert(desc.Format != ::DXGI_FORMAT_UNKNOWN && "Not allow buffer desc.");
			m_View.Format = Format;

			const auto subresourceCount{ desc.DepthOrArraySize * desc.MipLevels };
			m_Footprints.resize(subresourceCount);
			device->GetCopyableFootprints(&desc, 0u, subresourceCount, 0u, m_Footprints.data(), nullptr, nullptr, nullptr);

			SetView({ desc.MipLevels, });
			m_States.resize(subresourceCount, ::D3D12_RESOURCE_STATE_COMMON);
		}
		Texture(Device& device, Pointer<Heap> const& position, 
			::D3D12_RESOURCE_DESC const& desc, ::D3D12_SRV_DIMENSION dimension);
	public:
		template<typename ContextT>
		void Copy(ContextT& copy, ::std::shared_ptr<TextureCopy>);

		inline::std::span<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> 
			Footprints(::UINT base, ::UINT count = 0u) 
		{
			assert(!m_Footprints.empty() && "Footprints cannot be empty.");
			return{ m_Footprints.begin() + base, count ? count : m_Footprints.size() };
		}
		// Would not immediatly upload to descriptor handle.
		inline void MostDetailedMip(::UINT value)	{ m_View.Texture2D.MostDetailedMip = value; }
		// Would not immediatly upload to descriptor handle.
		inline void MipLevel(::UINT value)			{ m_View.Texture2D.MipLevels = value; }

		const::UINT64		ResourceSize;
		const::DXGI_FORMAT  Format;
	private:
		void SetView(ViewDesc const& desc);
	private:
		::std::shared_ptr<Resource> m_Resource;
		::std::vector<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> m_Footprints;
	};
	inline void Texture::SetView(ViewDesc const& desc)
	{
		switch (m_View.ViewDimension)
		{
		case D3D12_SRV_DIMENSION_TEXTURE2D:
			m_View.Texture2D
				= { desc.MostDetailedMip, desc.MipLevels, desc.PlaneSlice };
			break;
		// they have same structure layout, thus doing this is ok.
		case D3D12_SRV_DIMENSION_TEXTURE1D:
		case D3D12_SRV_DIMENSION_TEXTURE3D:
		case D3D12_SRV_DIMENSION_TEXTURECUBE:
			m_View.TextureCube
				= { desc.MostDetailedMip, desc.MipLevels };
			break;
		case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
		case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
		case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
			m_View.Texture2DArray = {};
			break;

		case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
		case D3D12_SRV_DIMENSION_TEXTURE2DMS:		assert(!"Mutli sample shader texture is not support.");
			break;
		case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
													assert(!"Ray tracing is not support currently.");
			break;
		case D3D12_SRV_DIMENSION_BUFFER:			assert(!"Not allow buffer view on texture.");
			break;
		case D3D12_SRV_DIMENSION_UNKNOWN:
		default:									assert(!"Not allow Unkown view type.");
			break;
		}
	}

}