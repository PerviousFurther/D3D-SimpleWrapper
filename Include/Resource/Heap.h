#pragma once

#include "Allocator.hpp"

namespace twen
{
	struct HeapExtraInfo
	{
		enum VaildAlignment : UINT32
		{
			Normal,
			MSAA,
		};

		VaildAlignment Alignment : 2 { Normal };
		::UINT VisibleNode : 8 { 0u };
		::D3D12_HEAP_FLAGS Flags{ ::D3D12_HEAP_FLAG_NONE };
	};

	// Should be noticed that when construct an heap. Expressing size will automatically align by alignemnt of <a>info</a>'s Alignment.
	class Heap
		: public inner::ShareObject<Heap>
		, inner::DeviceChild
		, public inner::MultiNodeObject
		, public Residency::Resident
	{
	public:

		using this_t = Heap;
		using interface_t = ::ID3D12Heap;
		using interface_pointer = ::ID3D12Heap*;
		using sort_t = ::D3D12_HEAP_TYPE;
		using extra_info = HeapExtraInfo;
		using pointer_t = inner::Pointer<this_t>;

	public:

		// Construct as non-UMA GPU heap.
		Heap(Device& device, sort_t type, ::UINT64 size, HeapExtraInfo const& info = {})
			: Resident
			{
			  resident::Heap
			, 1ull << (63u - ::std::countl_zero(size) + static_cast<bool>((size & (size - 1u))))
			, static_cast<bool>(info.Flags & ::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT)
			}
			, DeviceChild{ device }
			, MultiNodeObject{ info.VisibleNode, device.NativeMask }
			, IsUmaHeap{ false }
			, Type{ type }
			, Desc
			{
			.SizeInBytes{size},
			.Properties
			{
				.Type{type},
				.CreationNodeMask{device.NativeMask},
				.VisibleNodeMask{info.VisibleNode},
			},
			.Alignment
			{
				info.Alignment == HeapExtraInfo::MSAA
				? (UINT64)D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
				: (UINT64)D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
			},
			.Flags{info.Flags},
			}
		{
			MODEL_ASSERT(!(size % Desc.Alignment), "Size is not aligned.");

			device->CreateHeap(&Desc, IID_PPV_ARGS(&m_Heap));
			MODEL_ASSERT(m_Heap, "Create heap failure.");

		#if D3D12_MODEL_DEBUG
			m_DebugName = std::format(L"Heap_{}{}", Debug::Name(), ID);
			m_Heap->SetName(m_DebugName.data());
		#endif
		}
		// Construct as UMA cpu heap.
		Heap(Device& device, ::UINT64 size, ::D3D12_CPU_PAGE_PROPERTY properity, D3D12_MEMORY_POOL memoryPool, HeapExtraInfo const& info = {})
			: Resident
			{
			  resident::Heap
			, 1ull << (63u - ::std::countl_zero(size) + static_cast<bool>((size & (size - 1u))))
			, static_cast<bool>(info.Flags & ::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT)
			}
			, DeviceChild{ device }
			, MultiNodeObject{ info.VisibleNode, device.NativeMask }
			, IsUmaHeap{ false }
			, Type{ ::D3D12_HEAP_TYPE_CUSTOM }
			, Desc
			{
			.SizeInBytes{size},
			.Properties
			{
				.Type{::D3D12_HEAP_TYPE_CUSTOM},
				.CPUPageProperty{properity},
				.MemoryPoolPreference{memoryPool},
				.CreationNodeMask{device.NativeMask},
				.VisibleNodeMask{info.VisibleNode},
			},
			.Alignment
			{
				info.Alignment == HeapExtraInfo::MSAA
				? (UINT64)D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
				: (UINT64)D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
			},
			.Flags{info.Flags},
			}
		{
			MODEL_ASSERT(!device.SwitchToAdapter()->Desc.NonLocalMemoryInfo.Budget, "Only uma can create custom heap.");
			MODEL_ASSERT(!(size % Desc.Alignment), "Size is not aligned.");

			device->CreateHeap(&Desc, IID_PPV_ARGS(&m_Heap));
			MODEL_ASSERT(m_Heap, "Create heap failure.");
		}

		~Heap() { m_Heap->Release(); }

	public:

		inner::Pointer<Heap> Address();

		void Evict() const { GetDevice().Evict(this); }

		operator::ID3D12Heap* () const
		{
			return m_Heap;
		}

	public:

		const bool IsUmaHeap;
		const sort_t	Type;
		const::D3D12_HEAP_DESC Desc;

	private:
	#if D3D12_MODEL_DEBUG
		::std::wstring m_DebugName;
	#endif
		::ID3D12Heap* m_Heap;
		// Record range that was used.
		//::std::unordered_map<::UINT64, ::UINT64> m_RangeMap;
	};

	// inline definition.

}

namespace twen::inner
{
	// Do not const_cast on this.
	template<>
	struct inner::Pointer<Heap> : PointerBase<Heap>
	{
		using backing_t = Heap;

		::UINT64 Alignment;

		ID3D12Heap* operator*() const
		{
			return *Backing.lock();
		}

		inline inner::Pointer<Heap> Subrange(::UINT64 offset, ::UINT64 size) const
		{
			MODEL_ASSERT(offset + size <= Size, "Out of range.");
			return { Backing, Offset + offset, size, nullptr, Alignment };
		}
		friend bool operator==(inner::Pointer<Heap> const& left, inner::Pointer<Heap> const& right)
		{
			return /*left.Backing.owner_before(right.Backing) && */
				left.Offset == right.Offset
				&& left.Size == right.Size;
		}

	};
}


namespace twen 
{
	inline inner::Pointer<Heap> Heap::Address()
	{
		return{ share::weak_from_this(), 0u, Size, nullptr, Desc.Alignment, };
	}

}