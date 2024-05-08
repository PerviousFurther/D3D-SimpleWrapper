#include "mpch.h"

#include "Misc\Common.h"
#include "System\Device.h"

#include "Allocator.hpp"
#include "Heap.h"
#include "Resource.h"

namespace twen 
{
	//Resource::Resource(Device& device) : DeviceChild{ device }
	//{}

	CommittedResource::CommittedResource(Device& device, ::D3D12_HEAP_TYPE type, ::D3D12_RESOURCE_DESC const& desc,
		::D3D12_RESOURCE_STATES initState, ::UINT visible, ::D3D12_HEAP_FLAGS heapFlags, ::D3D12_CLEAR_VALUE const* optimizeValue)
		: Resource{desc, Committed}, MultiNodeObject{ device.NativeMask, visible }
	{
		assert(type !=::D3D12_HEAP_TYPE_CUSTOM && "This constructor is not allow custom heap.");

		::D3D12_HEAP_PROPERTIES heapProps{ type, ::D3D12_CPU_PAGE_PROPERTY_UNKNOWN, ::D3D12_MEMORY_POOL_UNKNOWN, device.NativeMask, visible };
		device->CreateCommittedResource(&heapProps, heapFlags, &desc, initState, optimizeValue, IID_PPV_ARGS(m_Handle.put()));
		assert(m_Handle && "Create resource failure.");
		SET_NAME(m_Handle, ::std::format(L"CommittedResource{}", ID));
	}

	//::std::vector<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> Resource::GetFootprints(::UINT index, ::UINT count, ::UINT offset)
	//{
	//	auto desc{ m_Handle->GetDesc() };
	//	::std::vector<::D3D12_PLACED_SUBRESOURCE_FOOTPRINT> result{ count };
	//	device->GetCopyableFootprints(&desc, index, count, offset, result.data(), nullptr, nullptr, nullptr);

	//	return result;
	//}
}
