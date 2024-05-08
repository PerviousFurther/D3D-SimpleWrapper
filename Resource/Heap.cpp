#include "mpch.h"

#include "System\Device.h"

#include "Allocator.hpp"
#include "Resource.h"

#include "Heap.h"

namespace twen 
{
	Heap::Heap(Device& device, ::D3D12_HEAP_TYPE type, ::UINT64 size,
		VaildAlignment alignment, NodeMask nodeMask, ::D3D12_HEAP_FLAGS flags)
		: DeviceChild{ device }
		, MultiNodeObject{ nodeMask.Visible, nodeMask.Creation }
		, Alignment{ static_cast<::UINT64>(alignment) }
		, Size{ 1ull << (64u - ::std::countl_zero(size) + ((size & (size - 1u)) ? 1u : 0u)) }
		, Type{ type }
	{
		assert(alignment >= static_cast<::UINT64>(Normal) && "Alignment too small.");
		assert(!(alignment & (alignment - 1)) && "Alignment must be power of 2.");
		assert(!(size % alignment) && "Size is not aligned.");

		::D3D12_HEAP_DESC desc{
			Size, { type, ::D3D12_CPU_PAGE_PROPERTY_UNKNOWN, ::D3D12_MEMORY_POOL_UNKNOWN, CreateMask, VisibleMask, },
			static_cast<::UINT>(alignment), flags
		};
		GetDevice()->CreateHeap(&desc, IID_PPV_ARGS(m_Heap.put()));
		assert(m_Heap && "Create heap failure.");

		SET_NAME(m_Heap, ::std::format(L"Heap{}", ID));
	}
}