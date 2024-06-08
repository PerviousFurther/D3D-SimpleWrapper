#pragma once

#include "Heap.h"

namespace twen::Utils 
{
	inline auto SubresourceIndex(::std::vector<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> const& resource, ::UINT64 offset, ::UINT64 size)
	{
		using footprint_t = D3D12_PLACED_SUBRESOURCE_FOOTPRINT;

		// resource is same, thus project the footprint by offset.
		auto proj
		{
			[](footprint_t const& foot)
			{
				return foot.Offset;
			}
		};
		// find left and right boundary.
		auto left = ::std::ranges::lower_bound(resource, offset, {}, proj);
		auto right = ::std::ranges::upper_bound(resource, offset + size, {}, proj);

		struct
		{
			::UINT Index;
			::UINT Count;
		}
		result{ static_cast<::UINT>(left - resource.begin()), static_cast<::UINT>(right - left) };
		return result;
	}
}

namespace twen
{
	// Resource would not actually create d3d12 resource. 
	// Use Resource::Create<[Placed/Reserved/Committed]Resource> to generate d3d12 resource.
	class Resource 
		: public inner::ShareObject<Resource>
		, inner::DeviceChild
		, public inner::MultiNodeObject
		, public Residency::Resident
	{
	public:

		enum sort_t { Committed, Placed, Reserved };

		using this_t            = Resource;
		using interface_t       = ::ID3D12Resource;
		using interface_pointer = interface_t*;
		using pointer_t         = inner::Pointer<this_t>;
		using footprint_t       =::D3D12_PLACED_SUBRESOURCE_FOOTPRINT;
		using desc_t            =::D3D12_RESOURCE_DESC;

	public:

		Resource(Resource const&) = delete;

	protected:

		Resource(Device& device, ::D3D12_RESOURCE_DESC const& desc, sort_t type, ::UINT visibleMask, bool isInitEvict)
			: Resident{ resident::Resource, 0u, isInitEvict }
			, DeviceChild{ device }
			, MultiNodeObject{ visibleMask, device.NativeMask }
			, Type{ type }
			, Desc{desc}
		{
			m_Footprints.resize(Utils::SubResourceCount(desc));
			device->GetCopyableFootprints(&desc, 0u, static_cast<::UINT>(m_Footprints.size()), 0u, m_Footprints.data(), nullptr, nullptr, &Size);
		}

		~Resource();
	public:

		::D3D12_GPU_VIRTUAL_ADDRESS BaseAddress() const 
		{ 
			MODEL_ASSERT(Desc.Dimension == ::D3D12_RESOURCE_DIMENSION_BUFFER, "Only buffer allow get address.");
			return m_Handle->GetGPUVirtualAddress(); 
		}

	public:
		// Call only the resource ** IS ** buffer.
		// Obtain subrange in [offset, offset + size).
		inner::Pointer<Resource> Address();

		// Device - ResidencyManager.
		void Evict() 
		{
			GetDevice().Evict(this);
		}

		// Call only the resource ** is NOT ** buffer.
		// obtain subresource range in [index, index + count).
		inner::Pointer<Resource> SubresourceOf(::UINT index, ::UINT count);

		// @param index: subresource index.
		footprint_t const& Footprints(::UINT index) const
		{ MODEL_ASSERT(m_Footprints.size() > index, "Out of range."); return m_Footprints.at(index); }

		auto Footprints() const
		{ return ::std::span{ m_Footprints.begin(), m_Footprints.end()}; }

		// DEBUG ONLY: memcmp the resource location with specified range.
		// *** Only for upload/readback buffer ***	
		void VerifyResourceLocation(inner::Pointer<Resource> const& location, ::std::span<::std::byte> bytes);
		// DEBUG ONLY: memcmp the resource location with specified range.
		// *** Only for upload/readback buffer ***	
		void VerifyResourceLocation(::D3D12_GPU_VIRTUAL_ADDRESS address, ::std::span<::std::byte> bytes);

		void UnMap(::std::span<::std::byte>) 
		{
			m_Handle->Unmap(0u, nullptr);

			m_MapCount--;
		}
		::std::span<::std::byte> Map(::UINT64 offset, ::UINT64 size) 
		{
			void* data;
			m_Handle->Map(0u, nullptr, &data);

			MODEL_ASSERT(data, "Should not map on this resource.");

			m_MapCount++;
			return{ ::std::bit_cast<::std::byte*>(data) + offset, size };
		}

		operator::ID3D12Resource* () const;

	public:

		const sort_t Type;
		const::D3D12_RESOURCE_DESC Desc;

	protected:

	#ifdef D3D12_MODEL_DEBUG
		::std::wstring m_DebugName;
	#endif // D3D12_MODEL_DEBUG
		// Footprints help copy operation.
		::std::vector<footprint_t> m_Footprints;
		::UINT m_MapCount{ 0u };
		::ID3D12Resource* m_Handle;
	};

	// Committed resource.
	class CommittedResource final : public Resource
	{
	public:
		CommittedResource(Device& device, ::D3D12_HEAP_TYPE type, ::D3D12_RESOURCE_DESC const& desc,
			::D3D12_RESOURCE_STATES initState, ::UINT visibleNode, ::D3D12_HEAP_FLAGS heapFlags,
			::D3D12_CLEAR_VALUE const& optimizeValue = {});
	};

	// Placed resource.
	class PlacedResource final : public Resource
	{
	public:
		static constexpr Resource::sort_t Type{Placed};
	public:
		PlacedResource(Device& device, inner::Pointer<Heap> const& address, ::D3D12_RESOURCE_DESC const& desc,
			::D3D12_RESOURCE_STATES initState, ::D3D12_CLEAR_VALUE const& optimizeValue = {});

		~PlacedResource()
		{
			BackingPosition.Fallback();
		}
	public:
		const inner::Pointer<Heap> BackingPosition;
	};

	// Reserved resource is not finished.
	class ReservedResource final
	{
	public:
	};

	template<> struct inner::Allocator<ReservedResource> {};
	template<> struct inner::Allocator<PlacedResource> {};
	template<> struct inner::Allocator<CommittedResource> {};
}

namespace twen::inner
{
	// Do not const_cast on this.
	template<>
	struct inner::Pointer<Resource>
	{
		::std::weak_ptr<Resource> Backing;

		::D3D12_GPU_VIRTUAL_ADDRESS Address;

		union
		{

#pragma warning(disable: 4201)

			struct // For buffer.
			{
				::UINT64 Size;
				::UINT64 Offset;
			};

			struct // For texture.
			{
				::UINT SubresourceIndex;
				::UINT NumSubresource;
				::UINT16 ArrayIndex;
				::UINT16 MipLevels;
			};

#pragma warning(default: 4201)
		};

		inner::Allocator<Resource>* AllocateFrom;

		void Fallback() const 
		{
			if (AllocateFrom)
				AllocateFrom->Free(*this);
			else if (StandAlone())
				Backing.lock()->Evict();
			else
				MODEL_ASSERT(false, "Neither the pointer is allocated nor represent resource itself.");
		}

		inline inner::Pointer<Resource> Subrange(::UINT64 offset, ::UINT64 size) const
		{
			MODEL_ASSERT(Address, "Subrange an texture make no sense.");
			MODEL_ASSERT(offset + size <= Size, "Out of range.");
			return
			{
			Backing,
			Address + offset,
			{
				size,
				Offset + offset,
			}
			};
		}

		inline inner::Pointer<Resource> Subresource(::UINT index, ::UINT num) const
		{
			MODEL_ASSERT(false, "Not implemented or wont implement.");
			MODEL_ASSERT(!Address, "Buffer wont have subresource.");
			MODEL_ASSERT(index + num <= NumSubresource, "Out of range.");
			return
			{
				/*.Backing{Backing},
				.Address{0u},
				.SubresourceIndex{SubresourceIndex + index},
				.NumSubresource{num},
				.MipLevels{MipLevels},*/
			};
		}

		bool IsBufferPointer() const { return Address; }

		bool StandAlone() const
		{
			if (Address) 
			{
				MODEL_ASSERT(!Backing.expired(), "Cannot delete on dead resource.");
				return Backing.lock()->Size == Size;
			}
			else
				return Backing.lock()->Footprints().size() == NumSubresource;
		}

		::D3D12_GPU_VIRTUAL_ADDRESS operator*() const { MODEL_ASSERT(Address, "Not allow dereference an texture pointer to get virtual address."); return Address; }

		operator bool() const { return Backing.use_count(); }

	};

	template<>
	inline bool operator==<Resource>(inner::Pointer<Resource> const& left, inner::Pointer<Resource> const& right)
	{
		return left.Backing.lock().get() == right.Backing.lock().get() &&
			// verify resource position.
			(right.Address == left.Address
				? left.Address
				// if buffer.
				  ? left.Offset == right.Offset && right.Size == left.Size
				  // if texture.
				  : left.SubresourceIndex == right.SubresourceIndex && right.NumSubresource == left.NumSubresource
				: false);
	}
}

// Definition

namespace twen 
{
	// resource.

	inline PlacedResource::PlacedResource(Device& device, ::twen::inner::Pointer<Heap> const& address, 
		::D3D12_RESOURCE_DESC const& desc, ::D3D12_RESOURCE_STATES initState, ::D3D12_CLEAR_VALUE const& optimizeValue)
		: Resource{device, desc, Placed, address.Backing.lock()->VisibleMask, address.Backing.lock()->Evicted }
		, BackingPosition{address}
	{
	#if D3D12_MODEL_DEBUG
		::UINT64 size;
		device->GetCopyableFootprints(&desc, 0u, 1u, 0u, nullptr, nullptr, nullptr, &size);
		MODEL_ASSERT(size <= address.Size, "Allocation too small.");
		MODEL_ASSERT(!(address.Offset & (address.Alignment - 1u)), "Must aligned by alignment.");
		MODEL_ASSERT(desc.Flags & ::D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE ?
			desc.Flags & ::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : true,
			"Deny shader resource can only use with depth stencil.");
	#endif // 

		device->CreatePlacedResource(
			*BackingPosition, 
			BackingPosition.Offset,
			&desc, initState, 
			optimizeValue.Format==::DXGI_FORMAT_UNKNOWN ? nullptr : &optimizeValue
			, IID_PPV_ARGS(&m_Handle));

		MODEL_ASSERT(m_Handle, "Create placed resource failure.");
	#if D3D12_MODEL_DEBUG
		m_DebugName = std::format(L"Resource_{}{}", Debug::Name(), ID);
		m_Handle->SetName(m_DebugName.data());
	#endif
	}

	inline CommittedResource::CommittedResource(Device& device, ::D3D12_HEAP_TYPE type, ::D3D12_RESOURCE_DESC const& desc,
		::D3D12_RESOURCE_STATES initState, ::UINT visible, ::D3D12_HEAP_FLAGS heapFlags, ::D3D12_CLEAR_VALUE const& optimizeValue)
		: Resource{ device, desc, Committed, visible, static_cast<bool>(heapFlags & ::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT) }
	{
		MODEL_ASSERT(type != ::D3D12_HEAP_TYPE_CUSTOM, "This constructor is not allow custom heap.");

		MODEL_ASSERT(desc.Flags & ::D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE ? 
			desc.Flags & ::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : true, 
			"Deny shader resource can only use with depth stencil.");

		::D3D12_HEAP_PROPERTIES heapProps
		{ 
		  type
		, ::D3D12_CPU_PAGE_PROPERTY_UNKNOWN
		, ::D3D12_MEMORY_POOL_UNKNOWN
		, CreateMask
		, VisibleMask 
		};

		device->CreateCommittedResource(
			&heapProps, 
			// Committed resource cannot set these flags.
			heapFlags & ~(::D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES | ::D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES), 
			&desc, 
			initState,
			optimizeValue.Format == ::DXGI_FORMAT_UNKNOWN ? nullptr : &optimizeValue,
			IID_PPV_ARGS(&m_Handle));
		MODEL_ASSERT(m_Handle, "Create resource failure.");
	#if D3D12_MODEL_DEBUG
		m_DebugName = std::format(L"Resource_{}{}", Debug::Name(), ID);
		m_Handle->SetName(m_DebugName.data());
	#endif
	}

	inline Resource::~Resource() 
	{
		m_Handle->Release();
	}

	// Call only the resource ** IS ** buffer.
	// Obtain subrange in [offset, offset + size).
	inline inner::Pointer<Resource> Resource::Address()
	{
		MODEL_ASSERT(Desc.Dimension == ::D3D12_RESOURCE_DIMENSION_BUFFER, "Texture cannot get address.");
		return
		{
			share::shared_from_this(),
			BaseAddress(),
			Size, 0u,
		};
	}

	// Call only the resource ** is NOT ** buffer.
	// obtain subresource range in [index, index + count).
	inline inner::Pointer<Resource> Resource::SubresourceOf(::UINT index, ::UINT count)
	{
		MODEL_ASSERT(Desc.Dimension != ::D3D12_RESOURCE_DIMENSION_BUFFER, "Buffer would have subresource.");
		
		return
		{
			.Backing{ shared_from_this() },
			.Address{ 0u },
			.SubresourceIndex{ index },
			.NumSubresource{ count },
			.ArrayIndex{ static_cast<::UINT16>(index / Desc.MipLevels) },
			.MipLevels{ static_cast<::UINT16>(Desc.MipLevels - index % Desc.MipLevels) },
		};
	}

	// DEBUG ONLY: memcmp the resource location with specified range.
	// *** Only for upload/readback buffer ***	
	inline void Resource::VerifyResourceLocation(inner::Pointer<Resource> const& location, ::std::span<::std::byte> bytes)
	{
#if D3D12_MODEL_DEBUG
		MODEL_ASSERT(location.Size == bytes.size(), "Size must match when cmp the range.");
		MODEL_ASSERT(location.Address, "Method only for buffer.");
		::std::byte* tar{ nullptr };
		m_Handle->Map(0u, nullptr, (void**)&tar);
		auto result = ::std::memcmp(tar + location.Offset, bytes.data(), location.Size);
		MODEL_ASSERT(!result, "Resource is different in speified range.");

		m_Handle->Unmap(0u, nullptr);
#endif // D3D12_MODEL_DEBUG
	}

	// DEBUG ONLY: memcmp the resource location with specified range.
	// *** Only for upload/readback buffer ***	
	inline void Resource::VerifyResourceLocation(::D3D12_GPU_VIRTUAL_ADDRESS address, ::std::span<::std::byte> bytes)
	{
#if D3D12_MODEL_DEBUG
		MODEL_ASSERT(address, "Address is 0.");

		::std::byte* tar{ nullptr };
		m_Handle->Map(0u, nullptr, (void**)&tar);
		auto offset = address - BaseAddress();
		auto result = ::std::memcmp(tar + offset, bytes.data(), bytes.size());
		MODEL_ASSERT(!result, "Resource is different in speified range.");

		m_Handle->Unmap(0u, nullptr);
#endif // D3D12_MODEL_DEBUG
	}

	inline Resource::operator::ID3D12Resource* () const
	{
		this->GetDevice().UpdateResidency(this);

		if (Type == Placed)
			this->GetDevice().UpdateResidency(static_cast<const PlacedResource*>(this)->BackingPosition.Backing.lock().get());

		return m_Handle;
	}

}
