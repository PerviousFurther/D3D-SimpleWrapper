#pragma once

#include "Misc\ObjectSet.h"

namespace twen 
{
	inline constexpr struct 
	{
		bool operator()(::D3D12_QUERY_HEAP_TYPE heapType, ::D3D12_QUERY_TYPE queryType) const;
	} CheckQueryTypeIsAllow;

	class QuerySet;

	template<>
	struct Pointer<QuerySet> 
	{
		::std::weak_ptr<QuerySet> BackingSet;
		const::UINT Offset;
		const::UINT	Capability;
		
		::D3D12_QUERY_TYPE Type;
		::std::weak_ptr<AccessibleBuffer> OutDestination; // Requires readback buffer. If check is redunctant, set this buffer directly.

		void Destination(::std::shared_ptr<AccessibleBuffer> pointer); // Set resource safely.
		operator bool() const { return OutDestination.expired() && BackingSet.expired(); }
	};

	struct DirectContext;

	class QuerySet : public ShareObject<QuerySet>, public DeviceChild, public SetManager<QuerySet>
	{
		friend struct SetManager<QuerySet>;
	public:
		using interface_t = ::ID3D12QueryHeap;
		using sort_t = ::D3D12_QUERY_HEAP_TYPE;
		using argument_t =::D3D12_QUERY_TYPE;
	public:
		QuerySet(Device& device, ::D3D12_QUERY_HEAP_TYPE type, ::UINT count);
		operator interface_t* const () const { return m_Handle.Get(); }
	public:
		const sort_t Type;		// Heap type.
	private:
		Pointer<QuerySet> AddressOf(::UINT offset, ::UINT count);
		void DiscardAt(Pointer<QuerySet> const& address);
	private:
		ComPtr<::ID3D12QueryHeap> m_Handle;
	};
	inline QuerySet::QuerySet(Device& device, ::D3D12_QUERY_HEAP_TYPE type, ::UINT count)
		: DeviceChild{ device }
		, SetManager{ count }
		, Type{ type }
	{
		assert(count && "Query heap cannot be zero size.");

		::D3D12_QUERY_HEAP_DESC desc{ Type, count, device.NativeMask, };
		device->CreateQueryHeap(&desc, IID_PPV_ARGS(m_Handle.Put()));

		assert(m_Handle && "Failed to create query heap.");
		SET_NAME(m_Handle, ::std::format(L"QueryHeap{}", ID));
	}

	inline Pointer<QuerySet> QuerySet::AddressOf(::UINT offset, ::UINT count)
	{
		return { weak_from_this(), offset, count };
	}
	inline void Pointer<QuerySet>::Destination(::std::shared_ptr<AccessibleBuffer> readbackBuffer)
	{
		// check not enabled.
		OutDestination = readbackBuffer;
	}
}