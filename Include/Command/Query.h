#pragma once


namespace twen
{
	inline constexpr struct
	{
		bool operator()(::D3D12_QUERY_HEAP_TYPE heapType, ::D3D12_QUERY_TYPE queryType) const;
	} CheckQueryTypeIsAllow;

	// Modify create query pointer on d3d12 heap.
	class QueryHeap
		: public inner::ShareObject<QueryHeap>
		, public inner::DeviceChild
		, public inner::MultiNodeObject
		, public Residency::Resident
	{
	public:

		using this_t = QueryHeap;
		using share = inner::ShareObject<this_t>;
		using interface_t = ::ID3D12QueryHeap;
		using sort_t = ::D3D12_QUERY_HEAP_TYPE;
		using argument_t = ::D3D12_QUERY_TYPE;

		TWEN_ISCA Alignment{ 1u };

	public:

		QueryHeap(Device& device, ::D3D12_QUERY_HEAP_TYPE type, ::UINT count, ::UINT visible = 0u)
			: DeviceChild{ device }
			, Resident{ resident::QueryHeap, count }
			, MultiNodeObject{ visible, device.NativeMask }
			, Type{ type }
		{
			MODEL_ASSERT(count, "Query heap cannot be empty.");

			::D3D12_QUERY_HEAP_DESC desc{ Type, count, VisibleMask };
			device->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_Handle));

			MODEL_ASSERT(m_Handle, "Failed to create query heap.");
		}

		QueryHeap(const QueryHeap&) = delete;

		void Evict() const { GetDevice().Evict(this); }

		inner::Pointer<QueryHeap> Address();

		operator interface_t* const () const { return m_Handle; }

	public:

		const sort_t Type; // Heap type.

	private:

		::ID3D12QueryHeap* m_Handle;
	};
}

namespace twen::inner 
{
	template<>
	struct Pointer<QueryHeap> : PointerBase<QueryHeap>
	{
		::D3D12_QUERY_TYPE Type;

		inline Pointer<QueryHeap> Subrange(::UINT64 offset, ::UINT64 size) const
		{
			MODEL_ASSERT(offset + size <= Size, "Out of range.");
			return { Backing, Offset + offset, size, nullptr, Type };
		}
	};
}

// Definition.

namespace twen 
{

	inline inner::Pointer<QueryHeap> twen::QueryHeap::Address()
	{
		return { weak_from_this(), 0u, Size, nullptr, ::D3D12_QUERY_TYPE_OCCLUSION, };
	}
}