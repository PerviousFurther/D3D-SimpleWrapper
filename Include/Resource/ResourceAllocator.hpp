

#pragma once

/*--------------EXPERIMENTAL:
 *	
 * 
 *
 *								*/

namespace twen
{
	// TODO: Too java.
	struct ResourceAllocator
	{
	public:

		using result_t = inner::Pointer<Resource>;

		// Generic allocator interface.
		template<typename Allocator, typename Data>
		struct MultiAllocator : Data
		{
			using backing_t = typename Allocator::backing_t;

			using Data::Data;

			MultiAllocator(MultiAllocator const&) = delete;
			MultiAllocator& operator=(MultiAllocator const&) = delete;

			MultiAllocator(MultiAllocator&&) = default;
			MultiAllocator& operator=(MultiAllocator&&) = default;

			inner::Pointer<backing_t> Allocate(Device* device, typename Data::param_t data)
			{
				::UINT64 size;
				if constexpr (!::std::is_same_v<typename Data::param_t, ::UINT64>)
					size = Data::Projection(device, data);
				else
					size = data;

				for (auto& allocator : Allocators)
				{
					if (allocator->HaveSpace(size))
					{
						auto heapPosition = allocator->Alloc(size);
						if (heapPosition.Size)
							return heapPosition;
					}
				}
				if constexpr (requires{ Data::alignment; })
					Allocators.emplace_back(new Allocator{ Data::Init(device, data), Data::alignment });
				else 
					Allocators.emplace_back(new Allocator{ Data::Init(device, data) });
				return Allocators.back()->Alloc(size);
			}

			void CleanUp()
			{
				::std::erase_if(
					Allocators,
					[](auto allocator) -> bool
					{
						if (allocator->Empty())
						{
							delete allocator;
							return true;
						}
						else return false;
					}
				);
			}

			::std::vector< inner::Allocator<backing_t>* > Allocators{};
		};

		struct HeapInit
		{
			using param_t = ::UINT64;

			HeapInit(::D3D12_HEAP_TYPE type, ::D3D12_HEAP_FLAGS flags, ::UINT64 minHeapSize, bool msaa = false)
				: HeapType{ type }
				, HeapFlags{ flags }
				, MinHeapSize{ minHeapSize }
				, IsMsaa{ msaa }
			{}

			// 
			::std::shared_ptr<Heap> Init(Device* device, ::UINT64 size)
			{
				size = size > MinHeapSize ? size : MinHeapSize;
				return device->Create<Heap>
				(
					static_cast<::D3D12_HEAP_TYPE>(HeapType),
					size,
					HeapExtraInfo
					{
					.Alignment{IsMsaa ? HeapExtraInfo::MSAA : HeapExtraInfo::Normal},
					.VisibleNode{device->AllVisibleNode()},
					.Flags{HeapFlags}
					}
				);
			}

			bool IsMsaa;
			::D3D12_HEAP_TYPE  HeapType;
			::D3D12_HEAP_FLAGS HeapFlags;
#if D3D12_MODEL_DEBUG
			::UINT64 MaxHeapSize;
#endif
			::UINT64 MinHeapSize;
		};

		struct BufferInit : MultiAllocator<inner::BuddyAllocator<Heap>, HeapInit>
		{
			using param_t = HeapInit::param_t;

			TWEN_ISCA alignment{1u};

			BufferInit(::D3D12_HEAP_TYPE type, ::UINT64 minHeapSize, ::UINT64 minResourceSize,
				::D3D12_HEAP_FLAGS heapFlags, ::D3D12_RESOURCE_FLAGS resourceflags,
				::D3D12_RESOURCE_STATES initState)
				: MultiAllocator{ type, heapFlags, minHeapSize }
				, MinResourceSize{ minResourceSize }
				, ResourceFlags{ resourceflags }
				, InitResourceState{ initState }
			{}

			::std::shared_ptr<Resource> Init(Device* device, ::UINT64 size)
			{
				auto position = MultiAllocator::Allocate(device, size);
				return device->Create<PlacedResource>
				(
					position,
					ResourceOrder(position.Size),
					InitResourceState,
					::D3D12_CLEAR_VALUE{}
				);
			}

			::D3D12_RESOURCE_FLAGS ResourceFlags;
			::D3D12_RESOURCE_STATES InitResourceState;
#ifdef D3D12_MODEL_DEBUG
			::UINT64 MaxResourceSize;
#endif // 
			::UINT64 MinResourceSize;
		};

		// for Upload buffer and read back buffer.
		using BufferAllocator = MultiAllocator<inner::BuddyAllocator<Resource>, BufferInit>;

		// for texture 1d/2d/3d[array].
		struct TextureAllocator : MultiAllocator<inner::BuddyAllocator<Heap>, HeapInit>
		{
			TextureAllocator(::D3D12_HEAP_FLAGS extraFlags, ::UINT64 minHeapSize, bool isMsaa = false)
				: MultiAllocator{ ::D3D12_HEAP_TYPE_DEFAULT, ::D3D12_HEAP_FLAG_DENY_BUFFERS | extraFlags, minHeapSize, isMsaa }
			{}

			// Not optimized allocate method.
			// Wont bundle texture up to an big map...
			result_t Allocate(Device* device, ::D3D12_RESOURCE_DESC const& desc, ::D3D12_RESOURCE_STATES initState, ::D3D12_CLEAR_VALUE const& clearValue)
			{
				::UINT64 size;
				auto numOfSubResource = Utils::SubResourceCount(desc);
				(*device)->GetCopyableFootprints(&desc, 0u, numOfSubResource, 0u, nullptr, nullptr, nullptr, &size);
				auto pointerHeap{ MultiAllocator::Allocate(device, size) };

				auto resource = device->Create<PlacedResource>
				(
					pointerHeap,
					desc,
					initState,
					clearValue
				);

				//TODO: Change when planning on texture bundle.
				return resource->SubresourceOf(0u, numOfSubResource);
			}
			
			// Optimized allocate method.
			// would bound some texture into a big texture.
			::std::vector<inner::Pointer<Resource>> Allocate(int, auto&&...UNFINISHED);
		};

		// for default buffer.
		struct DefualtBufferAllocator : MultiAllocator<inner::BuddyAllocator<Heap>, HeapInit>
		{
			DefualtBufferAllocator(::D3D12_HEAP_FLAGS extraFlags, ::UINT64 minHeapSize)
				: MultiAllocator{ ::D3D12_HEAP_TYPE_DEFAULT, extraFlags, minHeapSize, false }
			{}

			using allocator_set = ::std::vector< inner::Allocator<Resource>* >;

			result_t Allocate(Device* device, ::UINT64 size, ::D3D12_RESOURCE_FLAGS flags,
				::D3D12_RESOURCE_STATES initState)
			{
				auto it{ m_Allocators.find(flags) };
				if (it != m_Allocators.end())
					for (auto& allocator : it->second)
						if (allocator->HaveSpace(size))
							if (auto pointer{ allocator->Alloc(size) }; pointer.Size)
								return pointer;

				auto pointer{ MultiAllocator::Allocate(device, size) };
				auto resource = device->Create<PlacedResource>
					(
						pointer,
						ResourceOrder(size).SetFlags(flags),
						initState,
						::D3D12_CLEAR_VALUE{}
					);

				if (it != m_Allocators.end())
				{
					return it->second.emplace_back(new inner::BuddyAllocator<Resource>(resource))->Alloc(size);
				} else {
					return m_Allocators.emplace(flags, allocator_set{}).first->second.back()->Alloc(size);
				}
			}
	
			void CleanUp() 
			{
				::std::erase_if(
					m_Allocators,
					[](auto& pair) 
					{
						::std::erase_if(pair.second,
							[](inner::Allocator<Resource>* value) -> bool
							{
								if (value->Empty())
								{
									delete value;
									
									return true;
								}
								else return false;
							});

						return pair.second.empty();
					});
			}

			::std::unordered_map<::D3D12_RESOURCE_FLAGS, allocator_set> m_Allocators;
		};

	public:

		//
		// Constructors
		//

		ResourceAllocator(Device& device, ::UINT64 minHeapSize, ::UINT64 minResourceSize,
			[[maybe_unused]]::UINT heapIncrement, [[maybe_unused]]::D3D12_HEAP_FLAGS extraFlagsOnCreateHeap = ::D3D12_HEAP_FLAG_NONE,
			[[maybe_unused]]::D3D12_RESOURCE_FLAGS extraFlagsOnCreateResource = ::D3D12_RESOURCE_FLAG_NONE)
			: MinHeapSize{ minHeapSize }
			, MinResourceSize{ minResourceSize }
			, m_UavCounterAllocator // TODO: Remove after finish uav creation that push the counter front of the view.
			{
				new inner::SegmentAllocator<Resource>
				{
				device.Create<CommittedResource>
				(
					::D3D12_HEAP_TYPE_DEFAULT,
					ResourceOrder{4 * 1024u}.SetFlags(::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
					::D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					device.AllVisibleNode(),
					::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT
				)
				, sizeof(::UINT)
				}
			}
		{}

		ResourceAllocator(ResourceAllocator const&) = delete;
		ResourceAllocator& operator=(ResourceAllocator const&) = delete;


	public:
		////
		//.. Allocation.
		//

		// Allocate buffer.
		result_t Allocate(Device* device, ResourceOrder const& order,
			::D3D12_RESOURCE_STATES state, ::D3D12_CLEAR_VALUE const& value = {}, ::UINT nodeMask = 0u)
		{
			nodeMask = nodeMask ? nodeMask : device->AllVisibleNode();

			if (order.Dimension == ::D3D12_RESOURCE_DIMENSION_BUFFER) // allocate buffers
			{
				auto size = order.Width;
				auto type = order.HeapType;

				if (type == ::D3D12_HEAP_TYPE_UPLOAD)
					return m_BufferAllocatorUpload.Allocate(device, size);
				else if (type == ::D3D12_HEAP_TYPE_READBACK)
					return m_BufferAllocatorReadback.Allocate(device, size);
				else
					return m_BufferAllocatorDefault.Allocate(device, size, order.Flags, state);

			}
			else { // allocate texture.

				if (order.Alignment == Constants::SmallResourceAlignment)
					return m_TextureAllocatorSmall.Allocate(device, order, state, value);
				else if (order.SampleDesc.Count > 1u)
					return m_TextureAllocatorMsaa.Allocate(device, order, state, value);
				else
					return m_TextureAllocatorDefualt.Allocate(device, order, state, value);
			}
		}

		void CleanUp()
		{
			m_BufferAllocatorUpload.CleanUp();
			m_BufferAllocatorReadback.CleanUp();
			m_BufferAllocatorDefault.CleanUp();
			m_TextureAllocatorDefualt.CleanUp();
			MODEL_ASSERT(m_TextureAllocatorDefualt.Allocators.empty(), "Memory leak.");
			m_TextureAllocatorSmall.CleanUp();
			MODEL_ASSERT(m_TextureAllocatorSmall.Allocators.empty(), "Memory leak.");
			m_TextureAllocatorMsaa.CleanUp();
			MODEL_ASSERT(m_TextureAllocatorMsaa.Allocators.empty(), "Memory leak.");
		}

	private:
		::UINT64 MinHeapSize;
		::UINT64 MinResourceSize;

#ifdef D3D12_MODEL_DEBUG
		::UINT64 MaxResourceSize{};
		::UINT64 MaxHeapSize{};
#endif // D3D12_MODEL_DEBUG

		inner::Allocator<Resource>* m_UavCounterAllocator;

		BufferAllocator m_BufferAllocatorUpload
		{
		::D3D12_HEAP_TYPE_UPLOAD,
		MinHeapSize,
		MinResourceSize,
		::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT,
		::D3D12_RESOURCE_FLAG_NONE,
		::D3D12_RESOURCE_STATE_GENERIC_READ
		};
		BufferAllocator m_BufferAllocatorReadback
		{
		::D3D12_HEAP_TYPE_READBACK,
		MinHeapSize >> 2,
		MinResourceSize,
		::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT,
		::D3D12_RESOURCE_FLAG_NONE,
		::D3D12_RESOURCE_STATE_COPY_DEST
		};

		DefualtBufferAllocator m_BufferAllocatorDefault
		{
		::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT | ::D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS
		, MinHeapSize,
		};

		TextureAllocator m_TextureAllocatorSmall
		{
		::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT | ::D3D12_HEAP_FLAG_DENY_BUFFERS | ::D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES
		, MinHeapSize >> 2
		};
		TextureAllocator m_TextureAllocatorDefualt
		{
		::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT | ::D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES
		, MinHeapSize
		};
		TextureAllocator m_TextureAllocatorMsaa
		{
		::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT | ::D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES
		, MinHeapSize << 4
		, true // IsMsaa
		};

	};
}
