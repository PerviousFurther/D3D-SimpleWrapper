#pragma once

/*
 *------------EXPERIMENTAL:
 * 
 *								*/

namespace twen::Residency
{
	struct SyncManager;
	struct ResidencyManager;
	
	// Contains some message that can help manage residency.
	struct Resident
	{
		inline static::UINT Growing{ 0u };

		enum class resident : ::UINT8
		{
			Resource,
			Heap,
			DescriptorHeap,
			QueryHeap, 
			PipelineState
		};

		::ID3D12Pageable* Pageable() const noexcept;

		mutable bool Evicted{false};
		const resident ResidentType : 3;
		// Indicate evicted or not. 

		 //::UINT ReferenceCounting{1u};

		// Resident's size.
		::UINT64 Size;
		// Fence value of single sync point.
		::UINT64 LastTicket{ 0u };
		// Time stamp of process running.
		::UINT64 LastTimeStamp{ 0u };
		
		const::UINT ID{ Growing++ };

		/*::UINT AddRef() 
		{
			return InterlockedIncrement(&ReferenceCounting);
		}
		::UINT Release() 
		{
			if(!InterlockedDecrement(&ReferenceCounting))
				delete this;
		}*/
	};

	template<::twen::inner::congener<Residency::Resident> T>
	inline auto DeleteResident(Residency::Resident* pointer)
	{
		static_cast<T*>(pointer)->~T();
		return sizeof(T);
	}

	struct List 
	{
		using deleter_type = ::std::size_t(*)(Resident*);

		struct List* Prev {this};
		struct List* Next {this};

		deleter_type const Delete;
	};

	struct ResidencySet 
	{
		// record resident.
		::std::unordered_set<const Resident*> ReferencedResident;
	};

	// Residency part.
	struct ResidencyManager
		: inner::DeviceChild
	{
		friend class Device;
		using resident_t = Residency::Resident;
	private:
		ResidencyManager(Device& device)
			: DeviceChild{ device }
			, m_Queue{}
		{
			::LARGE_INTEGER interger;
			if(!::QueryPerformanceCounter(&interger))
				throw::std::logic_error{"Query performance counter failure."};
			m_InitTimeStamp = static_cast<::UINT64>(interger.QuadPart);
		};

		void MakeEvicted(::UINT id)
		{
			MODEL_ASSERT(m_Residents.contains(id), "Object must be resident.");
			MODEL_ASSERT(!m_Evicted.contains(id), "BUG: One object multi records.");

			auto node = m_Residents.at(id);
			auto list = reinterpret_cast<List*>(node) - 1u;

			list->Next = &m_Queue;
			list->Prev = m_Queue.Prev;

			m_Queue.Prev->Next = list;
			m_Queue.Prev = list;

			node->Evicted = true;
			m_Evicted.emplace(id, list);
		}
		void MakeResident(::UINT id) 
		{
			MODEL_ASSERT(!m_Evicted.empty(), "Nothing to resident.");
			MODEL_ASSERT(m_Evicted.contains(id), "Not inside evict set.");
			MODEL_ASSERT(!m_Residents.contains(id), "BUG: One object multi records.");

			auto node = m_Evicted.at(id);

			node->Prev->Next = node->Next;
			node->Next->Prev = node->Prev;
			node->Next = nullptr;
			node->Prev = nullptr;

			auto resident = reinterpret_cast<resident_t*>(node + 1u);
			resident->Evicted = false;
			m_Residents.emplace(id, resident);
		}
		// Request memory from resident set.
		template<inner::congener<Resident> T, typename...Args>
		T* NewResident(Device& device, Args&&...args) 
		{
			auto ptr = operator new(sizeof(T) + sizeof(List));
			auto list = new(ptr) List { nullptr, nullptr, &DeleteResident<T>, };
			auto result = new(list + 1u) T { device, ::std::forward<Args>(args)... };

			if (result->Evicted) 
			{
				list->Next = &m_Queue;
				list->Prev = m_Queue.Prev;

				m_Queue.Prev->Next = list;
				m_Queue.Prev = list;

				m_Evicted.emplace(result->ID, list);
			} else
				m_Residents.emplace(result->ID, result);

			return result;
		}

		void DeleteObject(List* list) 
		{
			MODEL_ASSERT(list != &m_Queue, "Must not head itself.");
			MODEL_ASSERT(list->Next && m_Queue.Prev, "Not linked.");

			list->Next->Prev = list->Prev;
			list->Prev->Next = list->Next;
			
			auto id = reinterpret_cast<resident_t*>(list + 1u)->ID;
			MODEL_ASSERT(m_Evicted.contains(id), "Must be evicted.");

			operator delete(list, sizeof(list) + list->Delete(reinterpret_cast<struct Resident*>(list + 1u)));
		}

	public:

		~ResidencyManager()
		{
			if (m_CurrentResidencySet) 
				delete m_CurrentResidencySet;
			MODEL_ASSERT(m_Residents.empty(), "Havent clean up.");
			DeleteObject();
		}

		bool Recording() const { return m_CurrentResidencySet; }

		void BeginRecordingResidentChanging() 
		{
			MODEL_ASSERT(!m_CurrentResidencySet, "Not support multi residency set currently.");

			m_CurrentResidencySet = new ResidencySet{};
		}

		void StorageResident() 
		{
			::std::vector<::ID3D12Pageable*> settled;
			for (auto& resident : m_CurrentResidencySet->ReferencedResident) 
			{
				auto id = resident->ID;
				MODEL_ASSERT(resident->Evicted 
					? m_Evicted.contains(id)
					: m_Residents.contains(id),
					"Record mismatch with phenomenon.");

				auto it = m_Evicted.find(id);
				if (it != m_Evicted.end()) 
				{
					MakeResident(id);
					m_Evicted.erase(it);
					settled.emplace_back(resident->Pageable());
					MODEL_ASSERT(!resident->Evicted, "Value is not set.");
				}
			}

			delete m_CurrentResidencySet;
			m_CurrentResidencySet = nullptr;

			if (settled.size()) 
				GetDevicePointer()->MakeResident(static_cast<::UINT>(settled.size()), settled.data());

			// TODO: Object in evicted state excceed threshold rate can be release.
		}

		void EvictAll() { ::std::erase_if(m_Residents, [this](auto const& pair) { MakeEvicted(pair.first); return true; }); }

		void DeleteObject() 
		{ 
			::std::erase_if(m_Evicted, [this](auto const& pair) { DeleteObject(pair.second); return true; });
		}

		void DeleteObject(const resident_t* resident)
		{
			auto id = resident->ID;
			MODEL_ASSERT(m_Evicted.contains(resident->ID) || m_Residents.contains(resident->ID), "BUG: object is not recorded.");

			if (!resident->Evicted)
			{
				MakeEvicted(id);
				m_Residents.erase(id);
			} else {
				DeleteObject(m_Evicted.at(id));
				m_Evicted.erase(id);
			}
		}

		template<::std::ranges::range Rng>
		void DeleteObject(Rng&& range) 
		{
			::std::ranges::for_each(range, 
				[this](auto& value) 
				{ 
					if constexpr (::std::convertible_to<resident_t*>) 
						 // for set and vector.
						DeleteObject(value);
					else // for map.
						DeleteObject(value.second);
				}); 
		}
	private:
		::UINT64 m_InitTimeStamp{};

		// temporary, before started multi thread programming.
		ResidencySet *m_CurrentResidencySet{nullptr};
		
		::std::unordered_map<::UINT, resident_t*> m_Residents;

		List m_Queue;
		::std::unordered_map<::UINT, List*> m_Evicted;
	};

}