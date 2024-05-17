#pragma once

#include "Allocator.hpp"

#include "Heap.h"

namespace twen 
{
	class Resource;

	using MappedDataT = ::std::span<::std::byte>;

	template<>
	struct Pointer<Resource> 
	{
		const::UINT64 Size;
		const::UINT64 Offset;

		mutable::std::weak_ptr<Resource> Resource;

		const::D3D12_GPU_VIRTUAL_ADDRESS Address;

		Allocator<::twen::Resource>& Allocator;

		operator bool() const { return !Resource.expired(); }
	};

	// Resource would not actually create d3d12 resource. 
	// Use Resource::Create<[Placed/Reserved/Committed]Resource> to generate d3d12 resource.
	class DECLSPEC_NOVTABLE Resource : public ShareObject<Resource>
	{
		friend struct Context;
	public:
		static::UINT64 GetAllocationSize(::D3D12_RESOURCE_DESC const& desc);
		static::D3D12_RANGE FootprintToRange(::std::vector<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> const& footprints);
	public:
		enum sort_t { Committed, Placed, Reserved };

		Pointer<Resource> AddressOf(::UINT64 offset, ::UINT64 size, Allocator<Resource>& allocator);
		void DiscardAt(Pointer<Resource> const& pointer);

		::D3D12_GPU_VIRTUAL_ADDRESS BaseAddress() const { return m_Handle->GetGPUVirtualAddress(); }
		::D3D12_RESOURCE_DESC GetDesc()			  const { return m_Handle->GetDesc(); }

		bool IsRangeLocked(::D3D12_RANGE range) const;
		void LockRange(::D3D12_RANGE range) { m_LockedRange.emplace(range.Begin, range.End); }

		template<typename Self>
		::D3D12_HEAP_PROPERTIES HeapProps(this Self&& self, ::D3D12_HEAP_FLAGS* flagsOut = nullptr);

		operator::ID3D12Resource* () const { return m_Handle.Get(); }
	public:
		const sort_t Type;
		const::UINT64 Size;
		const::UINT64 Alignment;
	protected:
		Resource(::D3D12_RESOURCE_DESC const& desc, sort_t type)
			: Size{ GetAllocationSize(desc) }, Alignment{ desc.Alignment }, Type{type}
		{}
	protected:
		ComPtr<::ID3D12Resource>	m_Handle;
		::std::map<::UINT64, ::UINT64> m_LockedRange; // TODO: lock a resource.
	};

	// Committed resource.
	class DECLSPEC_EMPTY_BASES CommittedResource : public Resource, public MultiNodeObject
	{
	public:
		CommittedResource(Device& device, ::D3D12_HEAP_TYPE type, ::D3D12_RESOURCE_DESC const& desc,
			::D3D12_RESOURCE_STATES initState, ::UINT visibleNode = 0u, ::D3D12_HEAP_FLAGS heapFlags = ::D3D12_HEAP_FLAG_NONE,
			::D3D12_CLEAR_VALUE const* optimizeValue = nullptr);
	private:
		//::std::unordered_set<Pointer<Resource>> m_Sub;
	};

	// Placed resource.
	class PlacedResource : public Resource
	{
	public:
		static constexpr Resource::sort_t Type{Placed};
	public:
		// Not recommanded.
		template<template<typename>typename AllocatorT>
		PlacedResource(Device& device, AllocatorT<Heap>& address, ::D3D12_RESOURCE_DESC const& desc,
			::D3D12_RESOURCE_STATES initState, ::D3D12_CLEAR_VALUE* optimizeValue = nullptr);

		PlacedResource(Device& device, Pointer<Heap> const& address, ::D3D12_RESOURCE_DESC const& desc,
			::D3D12_RESOURCE_STATES initState, ::D3D12_CLEAR_VALUE* optimizeValue = nullptr);

		~PlacedResource() override = default;
	public:
		::std::shared_ptr<Heap> ShareHeap() const { return m_BackingHeap; }
		Heap& BorrowHeap()					const { return *m_BackingHeap; }

		const Pointer<Heap>	HeapPointer;
	private:
		const::std::shared_ptr<Heap> m_BackingHeap;
	};

	// Reserved resource is not finished.
	class ReservedResource : public DeviceChild
	{
	public:
		ReservedResource(Device& device, ::std::vector<::std::shared_ptr<Heap>> const& aliasHeaps);
	private:
		::std::vector<::std::weak_ptr<Heap>> m_AliasHeaps;
		::std::vector<::D3D12_RANGE> m_Ranges;
	};

	// Reserved resource cannot be suballocate.
	template<> struct Allocator<ReservedResource> { Allocator(auto&&...) = delete; };
}

// inline 
namespace twen 
{
	// resources inline definition.

	// resource.

	inline::UINT64 Resource::GetAllocationSize(::D3D12_RESOURCE_DESC const& desc)
	{
		if (desc.Dimension == ::D3D12_RESOURCE_DIMENSION_BUFFER) return desc.Width;

		assert(desc.Format != ::DXGI_FORMAT_UNKNOWN && "Texture not allow unknown format.");
		auto singleSize =
			desc.Width * (desc.Format != ::DXGI_FORMAT_UNKNOWN ? Utils.GetBitPerPixel(desc.Format) >> 3 : 1)
			* desc.Height
			* desc.DepthOrArraySize;
		return Align.Align((singleSize + singleSize / 3) * desc.SampleDesc.Count, desc.Alignment, true);
	}
	inline ::D3D12_RANGE Resource::FootprintToRange(::std::vector<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> const& footprints)
	{
		// We assume footprints is contingous fragment.
		auto& back = footprints.back();
		return { footprints.front().Offset, back.Offset + back.Footprint.RowPitch * back.Footprint.Height * back.Footprint.Depth };
	}
	template<typename Self>
	inline::D3D12_HEAP_PROPERTIES Resource::HeapProps(this Self&& self, ::D3D12_HEAP_FLAGS* flagsOut)
	{
		static_assert(!::std::is_same_v<::std::decay_t<Self>, ReservedResource>, "Reserved resource is not allow to get heap props.");

		::D3D12_HEAP_PROPERTIES heapProps;
		m_Handle->GetHeapProperties(&heapProps, flagsOut);
		return heapProps;
	}
	inline bool Resource::IsRangeLocked(::D3D12_RANGE range) const
	{
		assert(range.Begin < range.End && "Invaild range.");
		assert(range.End <= Size && "Out of range.");

		auto it = m_LockedRange.lower_bound(range.Begin);
		if(it != m_LockedRange.end() && it->second > range.Begin) return true;

		it = m_LockedRange.upper_bound(range.End);
		if (it != m_LockedRange.end() && it->first < range.End) return true;

		return false;
	}
	FORCEINLINE Pointer<Resource> Resource::AddressOf(::UINT64 offset, ::UINT64 size, Allocator<Resource>& allocator)
	{
		return { size, offset, weak_from_this(), BaseAddress() + offset, allocator };
	}

	FORCEINLINE void Resource::DiscardAt(Pointer<Resource> const& pointer)
	{
		pointer.Resource.reset();
	}

	template<template<typename>typename AllocatorT>
	inline PlacedResource::PlacedResource(Device& device, AllocatorT<Heap>& allocator,
		::D3D12_RESOURCE_DESC const& desc, ::D3D12_RESOURCE_STATES initState, ::D3D12_CLEAR_VALUE* optimizeValue)
		: PlacedResource{device, allocator.Alloc(Resource::GetAllocationSize(desc)), desc, initState, optimizeValue} {}

	inline PlacedResource::PlacedResource(Device& device, ::twen::Pointer<Heap> const& address, 
		::D3D12_RESOURCE_DESC const& desc, ::D3D12_RESOURCE_STATES initState, ::D3D12_CLEAR_VALUE* optimizeValue)
		: Resource{desc, Placed}, m_BackingHeap{ address.Backing }
		, HeapPointer{address}
	{
		assert(Resource::GetAllocationSize(desc) > address.Size && "Allocation too small.");
		device->CreatePlacedResource(*HeapPointer, HeapPointer.Offset,
			&desc, initState, optimizeValue, IID_PPV_ARGS(m_Handle.Put()));
		assert(m_Handle && "Create placed resource failure.");
		SET_NAME(m_Handle, ::std::format(L"PlacedResource{}", ID));
	}

	inline CommittedResource::CommittedResource(Device& device, ::D3D12_HEAP_TYPE type, ::D3D12_RESOURCE_DESC const& desc,
		::D3D12_RESOURCE_STATES initState, ::UINT visible, ::D3D12_HEAP_FLAGS heapFlags, ::D3D12_CLEAR_VALUE const* optimizeValue)
		: Resource{ desc, Committed }, MultiNodeObject{ device.NativeMask, visible }
	{
		assert(type != ::D3D12_HEAP_TYPE_CUSTOM && "This constructor is not allow custom heap.");

		::D3D12_HEAP_PROPERTIES heapProps{ type, ::D3D12_CPU_PAGE_PROPERTY_UNKNOWN, ::D3D12_MEMORY_POOL_UNKNOWN, device.NativeMask, visible };
		device->CreateCommittedResource(
			&heapProps, 
			heapFlags & ~(::D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES | ::D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES), 
			&desc, 
			initState,
			optimizeValue, 
			IID_PPV_ARGS(m_Handle.Put()));
		assert(m_Handle && "Create resource failure.");
		SET_NAME(m_Handle, ::std::format(L"CommittedResource{}", ID));
	}
}

// utils help build resource desc.
namespace twen 
{
	inline constexpr struct
	{
		// Make Buffer desc.
		FORCEINLINE constexpr::D3D12_RESOURCE_DESC operator()(::UINT64 size, 
			::UINT alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			::D3D12_RESOURCE_FLAGS flags = ::D3D12_RESOURCE_FLAG_NONE) const noexcept
		{
			return::D3D12_RESOURCE_DESC
			{
				::D3D12_RESOURCE_DIMENSION_BUFFER, alignment,
				size, 1u, 1ui16, 1ui16, ::DXGI_FORMAT_UNKNOWN,
				{ 1u, 0u }, ::D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags
			};
		}
		// Make Texutre1D desc.
		FORCEINLINE constexpr::D3D12_RESOURCE_DESC operator()(::UINT64 width, ::DXGI_FORMAT format,
			::UINT16 mipLevel = 1u, ::UINT16 arraySize = 1u, ::D3D12_RESOURCE_FLAGS flags = ::D3D12_RESOURCE_FLAG_NONE) const noexcept
		{
			return::D3D12_RESOURCE_DESC
			{
				::D3D12_RESOURCE_DIMENSION_TEXTURE1D, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
				width, 1u, arraySize, mipLevel, format, { 1u, 0u }, D3D12_TEXTURE_LAYOUT_UNKNOWN,
				flags,
			};
		}
		// Texture2D desc.
		FORCEINLINE constexpr::D3D12_RESOURCE_DESC operator()(::UINT64 width, ::UINT height,
			::DXGI_FORMAT format, ::UINT16 mipLevel = 1u, ::UINT16 arraySize = 1u,
			::D3D12_RESOURCE_FLAGS flags = ::D3D12_RESOURCE_FLAG_NONE) const noexcept {
			return::D3D12_RESOURCE_DESC
			{
				::D3D12_RESOURCE_DIMENSION_TEXTURE2D, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
				width, height, arraySize, mipLevel, format, { 1u, 0u }, D3D12_TEXTURE_LAYOUT_UNKNOWN,
				flags,
			};
		}
		// For render target or depth stencil. 
		// Tips: Once the format can be reconized by Utils.GetDSVFormat, ALLOW_DEPTH_STENCIL is set. Else ALLOW_RENDER_TARGET is set.
		FORCEINLINE constexpr::D3D12_RESOURCE_DESC operator()(::UINT64 width, ::UINT height, ::DXGI_SAMPLE_DESC const& sampleDesc,
			::DXGI_FORMAT format,
			::D3D12_RESOURCE_FLAGS extra = ::D3D12_RESOURCE_FLAG_NONE) const noexcept {
			auto dsvFormat = Utils.GetDSVFormat(format);
			::D3D12_RESOURCE_FLAGS flags{ dsvFormat == DXGI_FORMAT_UNKNOWN ?
				::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : ::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL };
			return::D3D12_RESOURCE_DESC
			{
				::D3D12_RESOURCE_DIMENSION_TEXTURE2D, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
				width, height, 1u, 1u, dsvFormat == DXGI_FORMAT_UNKNOWN ? format : dsvFormat, sampleDesc, D3D12_TEXTURE_LAYOUT_UNKNOWN,
				extra | flags,
			};
		}
		// Texture3D desc.
		FORCEINLINE constexpr::D3D12_RESOURCE_DESC operator()(::UINT64 width, ::UINT height, ::UINT16 depth,
			::DXGI_FORMAT format, ::UINT16 mipLevel = 1u,
			::D3D12_RESOURCE_FLAGS flags = ::D3D12_RESOURCE_FLAG_NONE) const noexcept {
			return::D3D12_RESOURCE_DESC
			{
				::D3D12_RESOURCE_DIMENSION_TEXTURE3D, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
				width, height, depth, mipLevel, format, { 1u, 0u }, D3D12_TEXTURE_LAYOUT_UNKNOWN,
				flags,
			};
		}
		// Conservative size.
		FORCEINLINE constexpr::UINT64 operator()(::D3D12_RESOURCE_DESC const& desc) const
		{
			return desc.Width * desc.Height * desc.DepthOrArraySize * 2u;
		}
	} ResourceDesc{};

	inline constexpr struct 
	{
		FORCEINLINE constexpr auto operator()(::ID3D12Resource* resource, 
			::D3D12_RESOURCE_STATES stateBefore,::D3D12_RESOURCE_STATES stateAfter, 
			::UINT subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			::D3D12_RESOURCE_BARRIER_FLAGS flags =::D3D12_RESOURCE_BARRIER_FLAG_NONE) const
		{ return::D3D12_RESOURCE_BARRIER{ ::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, flags, { resource, subresourceIndex, stateBefore, stateAfter } }; }

		FORCEINLINE constexpr auto operator()(::ID3D12Resource* before, ::ID3D12Resource* after,
			::D3D12_RESOURCE_BARRIER_FLAGS flags = ::D3D12_RESOURCE_BARRIER_FLAG_NONE) const
		{ return::D3D12_RESOURCE_BARRIER{ ::D3D12_RESOURCE_BARRIER_TYPE_ALIASING, flags, {.Aliasing{before, after} } }; }

		FORCEINLINE constexpr::D3D12_RESOURCE_BARRIER operator()(::ID3D12Resource* uav,
			::D3D12_RESOURCE_BARRIER_FLAGS flags = ::D3D12_RESOURCE_BARRIER_FLAG_NONE)
		{ return::D3D12_RESOURCE_BARRIER{ ::D3D12_RESOURCE_BARRIER_TYPE_UAV, flags, {.UAV{uav}} }; }
	} ResourceBarrier{};
}

//template<typename AccessType, typename...Args>
//::std::shared_ptr<AccessType> Lock(::D3D12_RANGE const& range, Args&&...args) 
//{
//	assert(!m_Mapped.empty() && "Resource cannot be access.");
//	return::std::make_shared<AccessType>(
//		Pointer{ BaseAddress() + range.Begin, m_Mapped.subspan(range.Begin, range.End - range.Begin), weak_from_this() },
//		::std::forward<Args>(args)...);
//}