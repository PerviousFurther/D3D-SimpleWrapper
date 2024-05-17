#pragma once

#include <queue>

namespace twen 
{
	enum class AllocatorType
	{
		Unknown, Buddy, Bucket, Pool
	};

	//template<typename T, typename R>
	//concept Allocator = requires(T t)
	//{
	//	{ t.Alloc(::UINT64{}) } -> ::std::convertible_to<Pointer<R>>;
	//	t.Free(::std::declval<Pointer<R> const&>());
	//};

	template<typename R>
	concept Allocable = requires(R resource)
	{
		{ resource.Size } -> ::std::integral;
		{ resource.Alignment } -> ::std::integral;
	};

	// Use for warp type.
	template<typename R>
	struct Allocator
	{
		Allocator(AllocatorType type) : Type{ type } {}

		template<template<typename T>typename AllocatorT>
		AllocatorT<R>& As()& { assert(Type == AllocatorT<R>::Type && "Mismatching conversion."); return static_cast<AllocatorT<R>&>(*this); }
		template<template<typename T>typename AllocatorT>
		AllocatorT<R> const& As() const& { return static_cast<AllocatorT<R> const&>(*this); }

		Pointer<R> Alloc(::UINT size);
		void Free(Pointer<R> const& pointer);
	private:
		const AllocatorType Type;
	};

	template<typename RT>
	class BuddyAllocator : Allocator<RT>
	{
	public:
		using Backing = RT;

		struct Data
		{
			::UINT SizeOrder;
		};

		static constexpr auto Type{AllocatorType::Buddy};
	public:
		BuddyAllocator(::std::shared_ptr<RT> backing) 
			: Allocator<RT>{ Type }
			, m_Backing{ backing }
			, AlignmentOrder{ static_cast<::UINT>(::std::countl_one(backing->Alignment)) }
			, MaxOrder{ sizeof(backing->Size) * CHAR_BIT - ::std::countr_zero(backing->Size) }
		{
			assert(!(backing->Size & (backing->Size - 1u)) && "Size should be power of 2 to avoid debris.");
			assert(!(backing->Alignment & (backing->Alignment - 1)) && "Alignment must be power of 2.");

			m_FreeList.resize(MaxOrder);
			m_FreeList.at(MaxOrder - 1u).insert(0u);
		}

		BuddyAllocator(const BuddyAllocator&) = delete;

		void Clear() 
		{
			for (auto const& allocation : m_Allocations)
				Free(allocation);
		}
		~BuddyAllocator() { Clear(); }
	public:
		// Even you express zero, it still return an allocation of alignment.
		Pointer<Backing> Alloc(::UINT64 size)
		{
			const auto alignment = AlignmentOrder;
			auto order = SizeToOrder(size) >> alignment;
			auto offset = Modify(order);

			auto result = m_Allocations.emplace(m_Backing->AddressOf(offset << alignment, 1ull << order << alignment, *this));
			return *result.first;
		}

		template<typename UNFINISHED>
		bool Realloc(Pointer<Backing>& pointer, ::UINT newSize);

		// After free. The pointer should not be availble.
		void Free(Pointer<Backing> const& pointer)
		{
			assert(m_Allocations.contains(pointer) && "Allocation is not inside current allocator.");

			Modify(static_cast<::UINT>(pointer.Size >> AlignmentOrder), static_cast<::UINT>(pointer.Offset >> AlignmentOrder));

			m_Allocations.erase(pointer);

			if constexpr (requires(Backing rt) { rt.DiscardAt(pointer); })
				m_Backing->DiscardAt(pointer);
		}
	private:
		FORCEINLINE::UINT Modify(::UINT order)
		{
			assert(order <= MaxOrder && "Buddy allocator overflow.");

			auto& set{ m_FreeList.at(order - 1u) };
			auto offset{ 0u };
			if (set.empty())
			{
				// depart the block.
				offset = Modify(order + 1u);

				auto size = 1u << order;
				auto newOffset = size + offset;

				set.insert(newOffset);
			}
			else {
				auto it = set.begin();

				offset = *it;

				set.erase(it);
			}

			return offset;
		}
		FORCEINLINE void Modify(::UINT order, ::UINT offset)
		{
			auto block = 1 << order;
			auto buddy = block ^ offset;
			auto& set{ m_FreeList.at(order) };
			if (set.contains(buddy))
			{
				Modify(order + 1u, offset > buddy ? buddy : offset);
				set.erase(buddy);
			}
			else
				set.emplace(offset);
		}

		FORCEINLINE::UINT GetBuddy(::UINT offset, ::UINT order) const { return offset ^ order; }

		FORCEINLINE::UINT SizeToOrder(::UINT64 size) const
		{
			auto alignSize = Align(size, m_Backing->Alignment);
			bool increase = (alignSize - 1u) & alignSize; // if power of 2, it is false.
			return 64u - ::std::countl_zero(alignSize) + increase;
		}
		FORCEINLINE::UINT64 OrderToSize(::UINT index)			const { return 1ull << index; }
	public:
		struct PointerHash
		{
			::std::uint64_t operator()(Pointer<RT> const& target) const
			{ return::std::hash<::UINT64>{}(target.Offset); }
		};

		const::UINT MaxOrder;
		const::UINT AlignmentOrder;
	private:
		::std::vector<::std::unordered_set<::UINT>> m_FreeList;
		::std::unordered_set<Pointer<RT>, PointerHash>	m_Allocations;

		::std::shared_ptr<Backing> m_Backing;
	};

	// Unfinished.
	/*template<typename RT>
	struct BucketAllocator 
	{
	public:
		using Backing = RT;

		struct Data 
		{
			::UINT Index;
			::UINT Offset;
		};

		static constexpr auto MinResourceSize{ 64 * 1024ull };
		static constexpr auto Type{AllocatorType::Bucket};
	public:
		BucketAllocator(::std::shared_ptr<Backing> backing, ::UINT64 retention)
			: m_Backing{ backing }
			, Retention{ retention }
		{}

		Pointer<RT> Alloc(::UINT64 size) 
		{
			assert(!"Not implmented");
		}
		void Free(Pointer<RT> const& pointer) 
		{
			auto const& stub = pointer.AllocatorStub;
			assert(!"Not implmented");
		}
	public:
		const::UINT64 Retention;
	private:
		struct PointerHash 
		{
			::std::size_t operator()(Pointer<RT> const& pointer) const 
			{ return::std::hash<::std::size_t>{}(pointer.Offset); }
		};

		static constexpr auto Shift{6ull};
		static constexpr auto NumBuckets{22ull};
	private:
		FORCEINLINE::UINT64 Modify(::UINT64 size)
		{
			auto bucket{ Bucket(size) };
		}
		FORCEINLINE void Modify(::UINT64 size, ::UINT64 index, ::UINT64 offset);

		FORCEINLINE static::UINT64 BlockSize(::UINT64 bufferSize) 
		{
			constexpr auto min{ 1 << Shift };
			return bufferSize <= min ? min : ::std::bit_ceil(bufferSize);
		}
		FORCEINLINE static::UINT64 Bucket(::UINT64 bufferSize) 
		{
			const::UINT64 bucket{ sizeof(::UINT64) * CHAR_BIT - ::std::countl_zero(bufferSize) + ((bufferSize & (bufferSize - 1u)) ? 1u : 0u) };
			return bucket < Shift ? 0 : bucket - Shift;
		}
	private:
		::std::queue<::UINT64> m_Availbles[NumBuckets]{};
		::std::queue<::UINT64> m_Retried;
		::std::unordered_set<Pointer<RT>, PointerHash>	m_Allocations;

		::std::shared_ptr<Backing> m_Backing;
	};*/

	template<typename UNFINISHED>
	class PoolAllocator
	{};

	template<typename R>
	inline Pointer<R> Allocator<R>::Alloc(::UINT size)
	{
		switch (Type)
		{
		case AllocatorType::Buddy:
			return As<BuddyAllocator>().Alloc(size);
		case AllocatorType::Bucket:
			assert(!"Bucket allocator is not finished.");
			return{};
		case AllocatorType::Unknown:
		default:assert(!"Should not access here."); return{};
		}
	}

	template<typename R>
	inline void Allocator<R>::Free(Pointer<R> const& pointer)
	{
		switch (Type)
		{
		case AllocatorType::Buddy:
			As<BuddyAllocator>().Free(pointer);
			return;
		case AllocatorType::Bucket:
			assert(!"Bucket allocator is not finished.");
			return;
		case AllocatorType::Unknown:
		default:assert(!"Should not access here."); return;
		}
	}

}