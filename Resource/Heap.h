#pragma once

namespace twen
{
	inline constexpr struct
	{
		FORCEINLINE::UINT64 Align2(::UINT64 size, ::UINT64 align, bool up) const
		{
			assert(align || (align & (align - 1)) && "Align must be power of 2.");
			return (size + up * (align - 1u)) & ~(align - 1u);
		}
		FORCEINLINE::UINT64 Align(::UINT64 size, ::UINT64 align, bool up) const
		{
			return ((size + up * (align - 1u)) / align) * align;
		}
	} Align{};

	// Normal heap.
	class Heap : public ShareObject<Heap>, public DeviceChild, public MultiNodeObject
	{
	public:
		enum VaildAlignment
		{
			Normal = 65536u,
			MSAA = 4194304u,
		};

		using sort_t =::D3D12_HEAP_TYPE;
	public:
		// Non-UMA GPU heap.
		Heap(Device& device, ::D3D12_HEAP_TYPE type, ::UINT64 size, VaildAlignment alignment = Normal,
			NodeMask nodeMask = {}, ::D3D12_HEAP_FLAGS flags = ::D3D12_HEAP_FLAG_NONE);

		Pointer<Heap> AddressOf(::UINT offset, ::UINT size, Allocator<Heap>& allocator);
		void DiscardAt(Pointer<Heap> const& address);

		operator::ID3D12Heap*() const { return m_Heap.get();}
	public:
		const sort_t	Type;
		const::UINT64	Size;
		const::UINT64	Alignment;
	protected:
		ComPtr<::ID3D12Heap> m_Heap;

		//::std::vector<Pointer<Heap>> m_Placed; // Resource
	};


	// UMA supportted heap.
	class DECLSPEC_EMPTY_BASES UMAHeap : public Heap
	{
	public:
		UMAHeap(Device& device, ::D3D12_CPU_PAGE_PROPERTY, D3D12_MEMORY_POOL, ::UINT64 size, VaildAlignment alignment = Normal,
			NodeMask nodeMask = {}, ::D3D12_HEAP_FLAGS flags = ::D3D12_HEAP_FLAG_NONE);
	private:

	};

	template<>
	struct Pointer<Heap>
	{
		const::UINT64 Offset;
		const::UINT64 Size;
		const::UINT64 Alignment;

		mutable::std::weak_ptr<Heap> Backing;

		Allocator<Heap>& Allocator;

		ID3D12Heap* operator*() const;
		friend bool operator==(Pointer<Heap> const&, Pointer<Heap> const&);
	};

	// inline definition.

	FORCEINLINE Pointer<Heap> Heap::AddressOf(::UINT offset, ::UINT size, Allocator<Heap>& allocator)
	{
		return{ offset, size, Alignment, weak_from_this(), allocator, };
	}
	FORCEINLINE void Heap::DiscardAt(Pointer<Heap> const& address)
	{
		address.Backing.reset();
	}
	FORCEINLINE::ID3D12Heap* Pointer<Heap>::operator*() const
	{
		return *Backing.lock(); 
	}

}