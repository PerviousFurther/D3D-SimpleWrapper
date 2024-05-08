#pragma once

#include "Resource.h"

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
	struct DirectContext;
	struct CopyContext;
	struct ComputeContext;
	struct Context;

	class ShaderResource : public ShareObject<ShaderResource>, public DeviceChild
	{
	public:
		using sort_t =::D3D12_SRV_DIMENSION;

		static constexpr auto Default4Component{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING };

		static constexpr auto WriteState{::D3D12_RESOURCE_STATE_COPY_DEST};
		static constexpr auto ReadState {::D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE};
	public:
		FORCEINLINE::D3D12_SHADER_RESOURCE_VIEW_DESC const& ViewDesc() const { return m_ViewDesc; }

		void CreateView(Pointer<DescriptorSet> const& position, ::UINT index) const;

		void Bind(DirectContext& context, ::UINT index) const; // use for DirectContext::Bind.

		operator::ID3D12Resource*() const { return *m_Resource; }
		const sort_t Type;
	protected:
		// Obtain resource is create from this shader resource.
		ShaderResource(Device& device, ::D3D12_RESOURCE_DESC const& desc, sort_t type);
		ShaderResource(Device& device, Pointer<Heap> const& position, ::D3D12_RESOURCE_DESC const& desc, sort_t type);
	protected:
		::std::shared_ptr<Resource>		m_Resource;

		::D3D12_SHADER_RESOURCE_VIEW_DESC m_ViewDesc{ .Shader4ComponentMapping{Default4Component} };
	};
	FORCEINLINE ShaderResource::ShaderResource(Device& device, ::D3D12_RESOURCE_DESC const& desc, sort_t type)
		: DeviceChild{ device }, Type{ type }
		, m_Resource{ Resource::Create<CommittedResource>(device, ::D3D12_HEAP_TYPE_DEFAULT, desc, ::D3D12_RESOURCE_STATE_COMMON) }
	{}
	FORCEINLINE ShaderResource::ShaderResource(Device& device, Pointer<Heap> const& position,::D3D12_RESOURCE_DESC const& desc, sort_t type)
		: DeviceChild{ device }, Type{ type }
		, m_Resource{ Resource::Create<PlacedResource>(device, position, desc, ::D3D12_RESOURCE_STATE_COMMON) }
	{ assert(position.Backing.lock()->Type ==::D3D12_HEAP_TYPE_DEFAULT && "ShaderResource must create on default heap."); }


	// Gpu buffer.
	class Buffer : public ShaderResource
	{
	public:
		static constexpr auto Dimension =::D3D12_SRV_DIMENSION_BUFFER;
		static constexpr auto MaxElementCount = 16384u;
	public:
		//Buffer(Pointer<Resource> const& pointer) noexcept(!D3D12_MODEL_DEBUG);
		Buffer(Device& device, ::UINT64 size, ::UINT alignment = 65536u);
	public:
		FORCEINLINE void SetView(::D3D12_BUFFER_SRV const& srvDesc, ::DXGI_FORMAT format =::DXGI_FORMAT_UNKNOWN)
		{
			m_ViewDesc.Format = format;
			m_ViewDesc.Buffer = srvDesc;
		}
		// is buffer a typed buffer.
		bool Typed() const { return m_ViewDesc.Format != DXGI_FORMAT_UNKNOWN; }
	private:
		::D3D12_RESOURCE_STATES m_States;
	};

	// Texture(2D/3D)[array/cube].
	class Texture : public ShaderResource
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
		Texture(Device& device, ::D3D12_RESOURCE_DESC const& desc, ::D3D12_SRV_DIMENSION dimension)
			: ShaderResource{ device, desc, dimension }
			, Format{ desc.Format }
			, ResourceSize{ Resource::GetAllocationSize(desc) } {

			assert(ShaderResources::IsDimensionMatch(desc.Dimension, dimension) && "Texture dimension mismatch.");
			m_ViewDesc.ViewDimension = dimension;

			assert(desc.Format != ::DXGI_FORMAT_UNKNOWN && "Not allow buffer desc.");
			m_ViewDesc.Format = Format;

			const auto subresourceCount{ desc.DepthOrArraySize * desc.MipLevels };
			m_Footprints.resize(subresourceCount);
			device->GetCopyableFootprints(&desc, 0u, subresourceCount, 0u, m_Footprints.data(), nullptr, nullptr, nullptr);

			SetView({ desc.MipLevels, }, 0.0f);

			m_States.resize(subresourceCount, ::D3D12_RESOURCE_STATE_COMMON);
		}
		Texture(Device& device, Pointer<Heap> const& position, 
			::D3D12_RESOURCE_DESC const& desc, ::D3D12_SRV_DIMENSION dimension);

		//Texture(Device& device, Allocator auto& allocator, ::D3D12_RESOURCE_DESC const& desc, ::D3D12_SRV_DIMENSION dimension, float mipClamp = 0.0f)
		//	: ShaderResource{ device, allocator, desc }, 
		//	  m_Format{ desc.Format }, 
		//	  m_ResourceSize{Resource::GetAllocationSize(desc)} 
		//{
		//	assert(desc.Format !=::DXGI_FORMAT_UNKNOWN && "Not allow unknown format.");
		//	m_ViewDesc.Format = m_Format;
		//	const auto subresourceCount{ desc.DepthOrArraySize * desc.MipLevels };
		//	m_Footprints.resize(subresourceCount);
		//	device->GetCopyableFootprints(&desc, 0u, subresourceCount, 0u, m_Footprints.data(), nullptr, nullptr, nullptr);
		//	SetView({ dimension, 0u, desc.MipLevels, 0u }, mipClamp);
		//}
	public:
		FORCEINLINE::std::span<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> GetFootprints(::UINT base, ::UINT count = 1u) 
		{
			assert(!m_Footprints.empty() && "Footprints cannot be empty.");
			return{ m_Footprints.begin() + base, count };
		}
		// Would not immediatly upload to descriptor handle.
		FORCEINLINE void MostDetailedMip(::UINT value)	{ m_ViewDesc.Texture2D.MostDetailedMip = value; }
		// Would not immediatly upload to descriptor handle.
		FORCEINLINE void MipLevel(::UINT value)			{ m_ViewDesc.Texture2D.MipLevels = value; }
		FORCEINLINE::std::vector<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> const& Footprints() const { return m_Footprints; }
		
		::D3D12_RESOURCE_BARRIER Transition(::D3D12_RESOURCE_STATES nextState, 
			::UINT index = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, ::D3D12_RESOURCE_BARRIER_FLAGS flags =::D3D12_RESOURCE_BARRIER_FLAG_NONE);

		const::UINT64		ResourceSize;
		const::DXGI_FORMAT  Format;
	private:
		void SetView(ViewDesc const& desc, float clamp = 0.0f);
	private:
		::std::vector<::D3D12_RESOURCE_STATES>				m_States;
		::std::vector<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> m_Footprints;
	};

	FORCEINLINE void Texture::SetView(ViewDesc const& desc, float clamp)
	{
		switch (m_ViewDesc.ViewDimension)
		{
		case D3D12_SRV_DIMENSION_TEXTURE2D:
			m_ViewDesc.Texture2D
				= { desc.MostDetailedMip, desc.MipLevels, desc.PlaneSlice, clamp };
			break;
		// they have same structure layout, thus doing this is ok.
		case D3D12_SRV_DIMENSION_TEXTURE1D:
		case D3D12_SRV_DIMENSION_TEXTURE3D:
		case D3D12_SRV_DIMENSION_TEXTURECUBE:
			m_ViewDesc.TextureCube
				= { desc.MostDetailedMip, desc.MipLevels, clamp };
			break;
		#pragma region Assert
		case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
		case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
		case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:	assert(!"Texture array is not finished.");
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
		#pragma endregion 
		}
	}

	// reserved.
	//case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
	//	m_ViewDesc.Texture2DArray
	//		= { desc.MostDetailedMip, desc.MipLevels, desc.FirstArraySliceOrFace, desc.ArraySize, desc.PlaneSlice,clamp };
	//	break;
	//case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
	//case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
	//	m_ViewDesc.TextureCubeArray
	//		= { desc.MostDetailedMip, desc.MipLevels, desc.FirstArraySliceOrFace, desc.ArraySize,clamp };
	//	break;

	//struct ViewDesc
	//{
	//	::D3D12_SRV_DIMENSION Dimension;
	//
	//	::UINT MostDetailedMip;
	//	::UINT MipLevels;
	//	::UINT PlaneSlice;
	//	UINT FirstArraySliceOrFace;
	//	UINT ArraySize;
	//	UINT PlaneSlice;
	//	FLOAT ResourceMinLODClamp{ 0.0f };
	//};

	//class TextureArray : public ShaderResource 
	//{
	//public:

	//protected:
	//	const::DXGI_FORMAT Formats;
	//	const::UINT		   ArraySize;
	//};

}