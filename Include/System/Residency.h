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

		::ID3D12Pageable* Pageable() const;

		const resident ResidentType : 3;
		// Resident's size.
		::UINT64 Size;
		// Indicate evicted or not. 
		mutable bool Evicted : 1 {false};

		const::UINT ID{ Growing++ };

		// Fence value of single sync point.
		::UINT64 LastTicket{ 0u };
		// Time stamp of process running.
		::UINT64 LastTimeStamp{ 0u };
	};

	struct ResidencySet 
	{
		// TODO: add mutex probably.
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
		{};

		void AddResident(::std::shared_ptr<resident_t> resident)
		{
			if (!resident->Evicted) 
			{
				::LARGE_INTEGER count;
				::QueryPerformanceCounter(&count);
				resident->LastTimeStamp = static_cast<::UINT64>(count.QuadPart);
				m_Residents.emplace(resident->ID, resident);
			} else {
				m_Evicted.emplace(resident->ID, resident);
			}
		}
		void AddEvict(::std::shared_ptr<resident_t> resident)
		{
			m_Evicted.emplace(resident->ID, resident);
		}

		void Evict(::UINT id)
		{
			auto it = m_Residents.find(id);
			MODEL_ASSERT(it != m_Residents.end(), "Cannot evict speicfied object.");

			m_Evicted.emplace(*it);
			m_Residents.erase(it);
		}

		void Resident(::UINT id)
		{
			auto it = m_Evicted.find(id);
			MODEL_ASSERT(it != m_Evicted.end(), "Cannot resident specified object.");

			m_Residents.emplace(*it);
			m_Evicted.erase(it);
		}
		
	public:

		~ResidencyManager()
		{
			if (m_CurrentResidencySet) 
				delete m_CurrentResidencySet;
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
				MODEL_ASSERT(resident->Evicted 
					? m_Evicted.contains(resident->ID) 
					: m_Residents.contains(resident->ID),
					"Record mismatch with phenomenon.");

				auto it = m_Evicted.find(resident->ID);
				if (it != m_Evicted.end())
				{
					m_Residents.emplace(it->first, it->second);
					resident->Evicted = false;

					settled.emplace_back(resident->Pageable());

					m_Evicted.erase(it);
				}
			}

			delete m_CurrentResidencySet;
			m_CurrentResidencySet = nullptr;

			if (settled.size()) 
				GetDevicePointer()->MakeResident(static_cast<::UINT>(settled.size()), settled.data());

			// TODO: evicted on too long can be release.
		}

		// Suspend when device was waiting for reboot.
		// TODO: Not finished.
		void EvictAll() 
		{
			::std::ranges::for_each(m_Residents, mark_evicted);
			m_Evicted.merge(::std::move(m_Residents));
		}
		void DeleteObject() 
		{
			m_Evicted.clear();
		}
		void DeleteObject(const resident_t* resident)
		{
			auto id{resident->ID};
			if (m_Residents.contains(id))
			{
				auto node{ m_Residents.extract(id) };
				node.mapped()->Evicted = true;
				m_Evicted.emplace(node.key(), node.mapped());
			} else {
				MODEL_ASSERT(m_Evicted.contains(id), "The object is not been record in this manager.");
				m_Evicted.erase(id);
			}
		}

		template<::std::ranges::range Rng>
		void DeleteObject(Rng&& range) { ::std::ranges::for_each(range, [this](auto& value) { DeleteObject(value); }); }
	private:

		static void mark_resident(::std::pair<const::UINT64, ::std::shared_ptr<resident_t>>& value) 
		{
			value.second->Evicted = false;
		}
		static void mark_evicted(::std::pair<const::UINT64, ::std::shared_ptr<resident_t>>& value)
		{
			value.second->Evicted = true;
		}

		::LARGE_INTEGER m_InitTimeStamp;

		// temporary, before started multi thread programming.
		ResidencySet* m_CurrentResidencySet{nullptr};
		
		::std::unordered_map<::UINT64,
			::std::shared_ptr<resident_t>> m_Residents;

		//::std::list<resident_t*> m_Cache;
		::std::unordered_map<::UINT64,
			::std::shared_ptr<resident_t>> m_Evicted;
	};

}