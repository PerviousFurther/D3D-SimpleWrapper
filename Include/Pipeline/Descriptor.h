#pragma once


namespace twen
{
	class DescriptorHeap 
		: public inner::ShareObject<DescriptorHeap>
		, public inner::DeviceChild
		, public inner::MultiNodeObject
		, public Residency::Resident
	{
	public:

		using this_t = DescriptorHeap;
		using interface_t =::ID3D12DescriptorHeap;
		using sort_t = ::D3D12_DESCRIPTOR_HEAP_TYPE;
		using flag_t = ::D3D12_DESCRIPTOR_HEAP_FLAGS;
		using pointer_t = inner::Pointer<this_t>;

		static constexpr auto NumMaximumSamplerDescriptors{ 2048u };
		static constexpr auto Alignment{ 1u };
	public:

		inline DescriptorHeap(Device& device, sort_t type,
			::UINT numDescriptor, 
			::D3D12_DESCRIPTOR_HEAP_FLAGS flags = ::D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			::UINT visible = 0u)
			: DeviceChild{ device }
			, MultiNodeObject{ visible, device.NativeMask }
			, Resident{ resident::DescriptorHeap, numDescriptor * device->GetDescriptorHandleIncrementSize(type), }
			, Type{ type }
			, Size{ numDescriptor }
			, Stride{ device->GetDescriptorHandleIncrementSize(type) }
			, ShaderVisible( flags &::D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE )
		{
			::D3D12_DESCRIPTOR_HEAP_DESC desc{ type, numDescriptor, flags, VisibleMask };
			device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Handle));

			MODEL_ASSERT(m_Handle, "Create descriptor heap failure.");
		}

		DescriptorHeap(DescriptorHeap const&) = delete;

		// Call when residency manager attempt to remove this object.
		~DescriptorHeap() 
		{ m_Handle->Release(); }

	public:

		pointer_t Address();

		// Do not call on this method when not On debug Mode.
		auto DbgCpuAddress() const 
		{
		#if D3D12_MODEL_DEBUG
			return m_Handle->GetCPUDescriptorHandleForHeapStart().ptr;
		#else
			return {};
		#endif
		}

		// Do not call on this method when not On debug Mode.
		auto DbgGpuAddress() const 
		{
		#if D3D12_MODEL_DEBUG
			return m_Handle->GetGPUDescriptorHandleForHeapStart().ptr;
		#else
			return {};
		#endif
		}

		void Evict() const 
		{ 
			GetDevice().Evict(this); 
		}

		operator::ID3D12DescriptorHeap* () const 
		{
			GetDevice().UpdateResidency(this);
			return m_Handle; 
		}
	public:

		const sort_t Type;
		const bool	 ShaderVisible : 1;
		const::UINT  Size{};
		const::UINT  Stride{};
	private:
		::ID3D12DescriptorHeap* m_Handle;
	};

}

namespace twen::inner
{
	template<>
	struct inner::Pointer<DescriptorHeap> : PointerBase<DescriptorHeap>
	{
		using backing_t = DescriptorHeap;;

		::UINT64 CPUHandle;
		::UINT64 GPUHandle;

		// Descriptor increment.
		::UINT64 Alignment;

		void Fallback() const
		{
			MODEL_ASSERT(StandAlone() || AllocateFrom, "Not hosting pointer must have an allocator.");
			if (AllocateFrom)
				AllocateFrom->Free(*this);
			else if (StandAlone())
				Backing.lock()->Evict();
		}

		inline inner::Pointer<DescriptorHeap> Subrange(::UINT64 offset, ::UINT64 size) const
		{
			MODEL_ASSERT(offset + size <= Size, "Out of range.");
			return
			{
				Backing,
				Offset + offset,
				size,
				nullptr,
				CPUHandle + offset * Alignment,
				GPUHandle + offset * Alignment,
				Alignment,
			};
		}

		// Check if the pointer is vaild.
		// Descriptor heap was always being allocated. Thus only verify allocator is ok.
		inline bool Vaild() const { return AllocateFrom; }

		// obtain base cpu handle.
		inline operator::D3D12_CPU_DESCRIPTOR_HANDLE() const { return { CPUHandle }; }

		// obtain base gpu handle.
		inline operator::D3D12_GPU_DESCRIPTOR_HANDLE() const { return { GPUHandle }; }

		inline::D3D12_GPU_DESCRIPTOR_HANDLE operator()(::UINT index) const
		{
			MODEL_ASSERT(GPUHandle, "Current gpu handle cannot be access.");
			MODEL_ASSERT(Size, "Empty block is not allow to access.");
			MODEL_ASSERT(index < Size, "Access violation on descriptor heap.");
			return { GPUHandle + index * Alignment };
		}
		inline::D3D12_CPU_DESCRIPTOR_HANDLE operator[](::UINT index) const
		{
			MODEL_ASSERT(Size, "Empty block is not allow to access.");
			MODEL_ASSERT(index < Size, "Access violation on descriptor heap.");

			return { CPUHandle + index * Alignment };
		}

		// Not recommand to call this method.
		// Not recommand to call this method.
		bool StandAlone() const { return Backing.lock()->Size == Size; }

		friend bool operator==(inner::Pointer<DescriptorHeap> const& left, inner::Pointer<DescriptorHeap> const& right)
		{
			return left.Offset == right.Offset && left.Size == right.Size;
		}
	};
}

namespace twen 
{

	inline DescriptorHeap::pointer_t DescriptorHeap::Address()
	{
		return
		{
			share::weak_from_this(),
			0u,
			this->Size,
			nullptr,
			m_Handle->GetCPUDescriptorHandleForHeapStart().ptr,
			ShaderVisible ? (m_Handle->GetGPUDescriptorHandleForHeapStart().ptr) : 0u,
			Stride,
		};
	}

}

namespace twen 
{
	struct DescriptorManager
	{
		DescriptorManager(Device& device, ::UINT numRtv, ::UINT numDsv, ::UINT numCSU, ::UINT numSampler)
		{
			// We dont use staging descriptor heap...
			// so just an shader visible descriptor heap should be ok.
	#define TEMPORARY_CREATE(type, num, flag)\
			if(num) DescriptorHeaps.emplace\
			(\
				::D3D12_DESCRIPTOR_HEAP_TYPE_##type, \
				new inner::DemandAllocator{device.Create<DescriptorHeap>\
				(\
					::D3D12_DESCRIPTOR_HEAP_TYPE_##type, \
					num, \
					::D3D12_DESCRIPTOR_HEAP_FLAG_##flag, \
					device.AllVisibleNode()\
					\
				)}\
			)\

			TEMPORARY_CREATE(CBV_SRV_UAV, numCSU, SHADER_VISIBLE);
			TEMPORARY_CREATE(SAMPLER, numSampler, SHADER_VISIBLE);
			TEMPORARY_CREATE(RTV, numRtv, NONE);
			TEMPORARY_CREATE(DSV, numDsv, NONE);
		}
	#undef TEMPORARY_CREATE
		~DescriptorManager()
		{
			for (auto& [_, allocator] : DescriptorHeaps)
				delete allocator;
		}

		DescriptorManager(DescriptorManager const&) = delete;
		DescriptorManager& operator=(DescriptorManager const&) = delete;

		inner::Pointer<DescriptorHeap> GetDescriptors(::D3D12_DESCRIPTOR_HEAP_TYPE type, ::UINT num)
		{
			auto& descriptorHeap{ DescriptorHeaps.at(type) };
			auto pointer = descriptorHeap->Alloc(num);

			// compare count.
			MODEL_ASSERT((pointer.CPUHandle - descriptorHeap->Location().CPUHandle) / pointer.Alignment <= descriptorHeap->Location().Size, "Out of range.");
			MODEL_ASSERT(!((pointer.CPUHandle - descriptorHeap->Location().CPUHandle) % pointer.Alignment), "Not aligned.");
			MODEL_ASSERT(pointer.Size, "Somthings wrong in descriptor manager.");

			return pointer;
		}
		// TEMPORARY.
		::std::array<::ID3D12DescriptorHeap*, 2> GetHeaps() const 
		{ 
			return 
			{ 
				*DescriptorHeaps.at(::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->Resource(),
				*DescriptorHeaps.at(::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)->Resource(),
			};
		}
	private:
		::std::unordered_map<
			::D3D12_DESCRIPTOR_HEAP_TYPE,
			inner::Allocator<DescriptorHeap>* > DescriptorHeaps;
	};
}