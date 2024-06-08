#pragma once

namespace twen::inner
{	
	enum class AllocatorType
	{
		Unknown, Buddy, Demand, Segment
	};

	// Allocator base interface, which offer method that can allocate resource with knowing it actual type.
	template<typename R>
	struct Allocator /*: Allocator<void>*/
	{
	public:

		using backing_t = R;

	protected:
		struct AllocateInfo 
		{
			::UINT64 Offset;
			::UINT64 Size;
		};
	private:

		inline static auto Growing{0u};

	protected:

		Allocator(AllocatorType type, ::std::shared_ptr<R> backingRes) 
			: Type{type}
		#if D3D12_MODEL_DEBUG
			, Size{backingRes->Size}
		#endif
			, m_Backing{backingRes->Address()}
			, m_AvailbleSize{backingRes->Size}
		{}

		Allocator(AllocatorType type, inner::Pointer<R> position)
			: Allocator<void>{ position.Backing.lock()->ResidentType }
		#if D3D12_MODEL_DEBUG
			, Size{ position.Size }
		#endif
			, m_AvailbleSize{position.Size}
			, m_Backing{position}
		{}

		Allocator(Allocator const&) = delete;

	public:
		~Allocator() 
		{
			MODEL_ASSERT(m_Allocations.empty(), "All allocation must be free before deletion.");
			MODEL_ASSERT(m_AvailbleSize == Size, "Memory leak.");

			m_Backing.Fallback();
		}

		template<template<typename T>typename AllocatorT>
		AllocatorT<R>& Rebind()& 
		{ 
			MODEL_ASSERT(Type == AllocatorT<R>::Type, "Mismatching conversion."); 
			return static_cast<AllocatorT<R>&>(*this); 
		}

		bool Full()                   const { return m_AvailbleSize == 0u; }
		bool Empty()                  const { return m_Backing.Size == m_AvailbleSize; }
		bool HaveSpace(::UINT64 size) const { return size <= m_AvailbleSize; }

		inner::Pointer<R> const& Location()	  const { return m_Backing; }

		::std::shared_ptr<R> Resource() const { return m_Backing.Backing.lock(); }

		auto Alloc(::UINT64 size);
		void Free(inner::Pointer<R> const& pointer);

	public:
		//Allocator<void>* LastAllocator;

		const AllocatorType Type;
		const::UINT64 ID{ Growing++ };
	protected:

		inner::Pointer<R> m_Backing;

		struct hash 
		{
			::std::size_t operator()(inner::Pointer<R> const& pointer) const
			{
				return::std::hash<::std::size_t>{}(pointer.Offset);
			}
		};

		::std::unordered_set<inner::Pointer<R>, hash> m_Allocations;

	#if D3D12_MODEL_DEBUG
		::UINT64 Size;
	#endif
		::UINT64 m_AvailbleSize{};
	};

	// Buddy allocator.

	template<typename Backing>
	struct BuddyAllocator : Allocator<Backing>
	{
	public:
		TWEN_ISCA Type{AllocatorType::Buddy};
	public:

		// Full allocate.
		BuddyAllocator(::std::shared_ptr<Backing> res, ::UINT64 alignment)
			: Allocator<Backing>{ Type, res }
			, m_AlignmentOrder{ 63u - ::std::countl_zero(alignment) }
		{ 
			MODEL_ASSERT(!(alignment& (alignment - 1u)), "Alignment must be power of 2.");
			UpdateSize(res->Size); 
		}

		// Full allocate.
		BuddyAllocator(::std::shared_ptr<Backing> res)
			: Allocator<Backing>{ Type, res }
			, m_AlignmentOrder{ sizeof(res->Desc.Alignment) * CHAR_BIT - ::std::countl_zero(res->Desc.Alignment) - 1u }
		{ UpdateSize(res->Size); }

		// Sub allocate.
		BuddyAllocator(inner::Pointer<Backing> const& position) 
			: Allocator<Backing>{ Type, position }
			, m_AlignmentOrder{ sizeof(position.Alignment) * CHAR_BIT - ::std::countl_zero(position.Alignment) - 1u }
		{ UpdateSize(position.size); }

		BuddyAllocator(const BuddyAllocator&) = delete;

	public:
		typename Allocator<Backing>::AllocateInfo Alloc(::UINT64 size)
		{
			size = Utils::Align2(size, 1ull << m_AlignmentOrder, true);
			auto order = SizeToOrder(size) - m_AlignmentOrder;
			auto offset = Find(static_cast<::UINT>(order));

			return { offset << m_AlignmentOrder, 1ull << order << m_AlignmentOrder};
		}

		bool Realloc(inner::Pointer<Backing>& pointer, ::UINT newSize) 
		{
			MODEL_ASSERT(false, "Not implemented.");
			return false;
		}

		void Free(auto const& pointer)
		{
			Modify(SizeToOrder(pointer.Size) >> m_AlignmentOrder, pointer.Offset);
		}

		void UpdateSize(::UINT64 size) 
		{
			MODEL_ASSERT(!(size & (size - 1u)), "Size should be power of 2 to avoid debris.");

			m_FreeList.clear();

			auto maxOrder{ 64u - ::std::countl_zero(size) };
			// We wont allocate block smaller that alignment
			m_FreeList.resize(maxOrder - m_AlignmentOrder);
			// Place start at biggest size block.
			m_FreeList.at(maxOrder - 1u - m_AlignmentOrder).insert(0u);
		}

	private:
		// on alloc.
		inline::UINT64 Find(::UINT order)
		{
			MODEL_ASSERT(m_FreeList.size() > order, "Buddy allocator overflow.");

			auto& set{ m_FreeList.at(order) };
			auto offset{ 0ull };
			if (set.empty())
			{
				offset = Find(order + 1u);

				// Depart the block into small one.
				auto size = 1u << order;
				auto newOffset = size + offset;

				set.insert(newOffset);
			} else {
				auto it = set.begin();

				offset = *it;

				set.erase(it);
			}

			return offset;
		}
		// on free.
		inline void Modify(::UINT order, ::UINT64 offset)
		{
			auto block = 1 << order;
			auto buddy = block ^ offset;

			MODEL_ASSERT(m_FreeList.size() > order, "Out of range.");

			auto& set{ m_FreeList.at(order) };
			if (set.contains(buddy))
			{
				Modify(order + 1u, offset > buddy ? buddy : offset);
				set.erase(buddy);
			}
			else
				set.emplace(offset);
		}

		inline static::UINT GetBuddy(::UINT offset, ::UINT order) { return offset ^ order; }

		inline static::UINT SizeToOrder(::UINT64 size)
		{
			auto order = 63u - ::std::countl_zero(size);
			return order + static_cast<bool>(size & (size - 1u));
		}
		inline static::UINT64 OrderToSize(::UINT index) { return 1ull << index; }

	private:
		::UINT64 m_AlignmentOrder;
		::std::vector<::std::unordered_set<::UINT64>> m_FreeList;
	};

	// Demand allocator.

	template<typename Backing>
	struct DemandAllocator 
		: public Allocator<Backing>
	{
	public:
		TWEN_ISCA Type{AllocatorType::Demand};

	public:

		DemandAllocator(::std::shared_ptr<Backing> res)
			: Allocator<Backing>{ Type, res }
		{ UpdateSize(res->Size, res->Alignment); }

		// Create as suballocation allocator part.
		DemandAllocator(inner::Pointer<Backing> const& position) 
			: Allocator<Backing>{ Type, position }
		{ UpdateSize(position.Size, position.Alignment); }

	public:

		typename Allocator<Backing>::AllocateInfo Alloc(::UINT64 desiredSize)
		{
			// it is size map.
			auto it = m_SizeMap.lower_bound(desiredSize);
			if (it == m_SizeMap.end())
			{
				MODEL_ASSERT(false, "Cannot find availble block.");
				return {};
			}

			MODEL_ASSERT(it->second.size(), "Offset set is empty.");

			desiredSize = Utils::Align(desiredSize, Alignment, true);

;			auto offsetIt = it->second.begin();
			auto offset   = *offsetIt;
			auto size     = it->first;
			
			m_OffsetMap.erase(offset);

			it->second.erase(offsetIt);
			if (it->second.empty()) 
				m_SizeMap.erase(it);

			AddNode(size - desiredSize, offset + desiredSize);
			
			return { offset, desiredSize };
		}

		bool Realloc(inner::Pointer<Backing>& pointer, ::UINT64 desiredSize)
		{
			MODEL_ASSERT(false, "Not implmented.");

			return false;
		}

		void Free(inner::Pointer<Backing> const& pointer)
		{
			::UINT64 offset = pointer.Offset;
			::UINT64 size = pointer.Size;

			MODEL_ASSERT(!m_OffsetMap.contains(offset), "Wrong place.");

			auto front = m_OffsetMap.lower_bound(offset);
			auto back = m_OffsetMap.empty() ? m_OffsetMap.begin() : front--;
			if (back != m_OffsetMap.end() && back->first == offset + size)
			{
				size += back->second;

				// current size would contains in size map.
				auto it = m_SizeMap.find(back->second);
				MODEL_ASSERT(it != m_SizeMap.end(), "Unexcepted phenomenon in free.");

				it->second.erase(back->first);
				if (it->second.empty()) 
					m_SizeMap.erase(it);

				m_OffsetMap.erase(back);
			}
			if (front != m_OffsetMap.end() && front->first + front->second == offset)
			{
				offset = front->first;
				size += front->second;

				auto it = m_SizeMap.find(front->second);
				MODEL_ASSERT(it != m_SizeMap.end(), "Unexcepted phenomenon in free.");

				it->second.erase(front->first);
				if (it->second.empty())
					m_SizeMap.erase(it);

				m_OffsetMap.erase(front);
			}

			AddNode(size, offset);
		}

		void UpdateSize(::UINT64 size, ::UINT64 alignment)
		{
			MODEL_ASSERT(!(size % alignment), "Must be aligned size.");
			Alignment = alignment;

			m_OffsetMap.emplace(0, size);
			m_SizeMap.emplace(size, ::std::set<::UINT64>{});
			m_SizeMap.at(size).emplace(0u);
		}

	private:

		void AddNode(::UINT64 size, ::UINT64 offset) 
		{
			m_OffsetMap.emplace(offset, size);

			if (auto it = m_SizeMap.find(size);
				it != m_SizeMap.end())
				it->second.emplace(offset);
			else
				m_SizeMap.emplace(size, ::std::set<::UINT64>{}).first->second.emplace(offset);
		}

	private:
		::UINT64 Alignment{0u};
		::std::map<::UINT64, ::UINT64> m_OffsetMap;
		::std::map<::UINT64, ::std::set<::UINT64>> m_SizeMap;
	};

	// Segment allocator.

	template<typename Backing>
	class SegmentAllocator
		: public Allocator<Backing>
	{
	public:
		
		TWEN_ISCA Type{ AllocatorType::Segment };

	public:

		SegmentAllocator(::std::shared_ptr<Backing> res, ::UINT chunkSize) 
			: Allocator<Backing>{ AllocatorType::Segment, res }
			, ChunkSize{ chunkSize }
		{ UpdateSize(res->Size);}

		SegmentAllocator(inner::Pointer<Backing> position, ::UINT chunkSize)
			: Allocator<Backing>{ AllocatorType::Segment, position }
			, ChunkSize{ chunkSize }
		{ UpdateSize(position.Size); }

	public: 

		Allocator<Backing>::AllocateInfo Alloc(::UINT64 size)
		{
			if (m_FreeList.empty())
				return {};
			
			auto it{ m_FreeList.end() - 1u };
			auto offset = it->Pop(size);

			if (it->Full(ChunkSize))
			{
				m_FullList.emplace(it->Index, ::std::move(*it));
				m_FreeList.erase(it);
			}

			return {offset, ChunkSize};
		}
		void Free(inner::Pointer<Backing> const& pointer)
		{
			MODEL_ASSERT(false, "Not implemented yet.");

			auto offset = pointer.Offset - m_Backing.Offset;
			auto index = offset / ChunkSize;

			auto it{ m_FullList.find(index) };
			if (it != m_FullList.end()) 
			{
				auto&& [_, chunk]{*it};
				chunk.Offsets.push(offset);
				m_FreeList.insert(m_FreeList.begin() + index, ::std::move(chunk));
				m_FullList.erase(it);
			} else m_FreeList.at(index).Offsets.push(offset);
		}
		void UpdateSize(::UINT64 size) 
		{
			MODEL_ASSERT(!(size % ChunkSize), "Should be aligned to decrease wasting.");
			MODEL_ASSERT(m_FullList.empty(), "Sth was not free.");
			m_FreeList.clear();

			auto count = size / ChunkSize;

			m_FullList.reserve(count);
			m_FreeList.resize(count);
			for (auto i{ 0u }; auto& chunk : m_FreeList)
				chunk.Index = i++;
		}

	public:

		const::UINT64 ChunkSize;

	private:
		struct Chunk 
		{
			::UINT64 Pop(::UINT64 size)
			{
				if (Offsets.empty()) 
				{
					auto stub = Back;
					Back += size;
					return stub;
				} else {
					auto offset = Offsets.front();
					Offsets.pop();
					return offset;
				}
			}
			bool Full(::UINT64 size) 
			{
				MODEL_ASSERT(size > Back, "Out of range in segment allocator.");
				return Back == size;
			}

			::UINT Index;
			::UINT64 Back{0u};

			::std::queue<::UINT64> Offsets{};
		};
	private:
		using Allocator<Backing>::m_Backing;
		::std::vector<Chunk> m_FreeList;
		::std::unordered_map<::UINT64, Chunk> m_FullList;
	};

	template<typename R>
	struct Fallback
	{
		void* Allocator;
		void(*Deleter)(inner::Pointer<R> const*, void*);
	};

	template<typename R>
	void FreeOnSingleAllocator(inner::Pointer<R> const* pointer, void* allocator)
	{
		MODEL_ASSERT(pointer && allocator, "Call on nullptr is invaild.");
		static_cast<inner::Allocator<R>*>(allocator)->Free(*pointer);
	}

	template<typename R>
	inline auto Allocator<R>::Alloc(::UINT64 size)
	{
		MODEL_ASSERT(m_AvailbleSize >= size, "No memory");

		AllocateInfo info{};
		switch (Type)
		{
		case AllocatorType::Buddy:
			info = Rebind<BuddyAllocator>().Alloc(size);
		break;
		case AllocatorType::Demand:
			info = Rebind<DemandAllocator>().Alloc(size);
		break;
		case AllocatorType::Segment:
			info = Rebind<SegmentAllocator>().Alloc(size);
		break;
		case AllocatorType::Unknown: MODEL_ASSERT(false, "Unknown allocator type.");
		}

		auto p = m_Backing.Subrange(info.Offset, info.Size);

		p.AllocateFrom = this;

		auto [result, success] {m_Allocations.emplace(p)};
		MODEL_ASSERT(success, "Emplace allocation failure.");
		m_AvailbleSize -= info.Size;

		return *result;
	}

	template<typename R>
	inline void Allocator<R>::Free(inner::Pointer<R> const& pointer)
	{
		switch (Type)
		{
		case AllocatorType::Buddy:
			Rebind<BuddyAllocator>().Free(pointer);
			break;
		case AllocatorType::Segment:
			Rebind<SegmentAllocator>().Free(pointer);
			break;
		case AllocatorType::Demand:
			Rebind<DemandAllocator>().Free(pointer);
			break;
		case AllocatorType::Unknown: MODEL_ASSERT(false, "Unknown allocator type.");
		}

		MODEL_ASSERT(m_Allocations.contains(pointer), "Must allocate from.");

		m_AvailbleSize += pointer.Size;
		m_Allocations.erase(pointer);
	}

}