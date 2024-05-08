#pragma once

// Sync utility.
namespace twen 
{
	struct CriticalSection
	{
		CriticalSection() { ::InitializeCriticalSectionAndSpinCount(&CS, 8u); }
		~CriticalSection() { ::DeleteCriticalSection(&CS); }

		CRITICAL_SECTION CS;
	};

	struct Event 
	{
		Event() :Handle{ ::CreateEventExW(nullptr, nullptr, NULL, EVENT_ALL_ACCESS) } {}
		~Event() { ::CloseHandle(Handle); }

		void Set() { ::SetEvent(Handle); }
		void Reset() { ::ResetEvent(Handle); }

		void Wait(DWORD milisecond = INFINITE) const 
		{
			WaitForSingleObject(Handle, milisecond);
		}
		::HANDLE Handle;
	};
	template<typename>
	struct ScopeLock;

	template<>
	struct ScopeLock<void>
	{
		ScopeLock(Event& event) 
			: Event{ &event }
			, Deletion{&ScopeLock::ResetEvent}
		{ event.Set(); }

		ScopeLock(CriticalSection& cs)
			: CS{ &cs } 
			, Deletion{ &ScopeLock::LeaveCS }
		{ ::EnterCriticalSection(&cs.CS); }

		~ScopeLock() { (this->*Deletion)(); }

		void LeaveCS() { ::LeaveCriticalSection(&CS->CS); }
		void ResetEvent() { Event->Reset(); }

		void (ScopeLock::* Deletion)();
		union 
		{
			CriticalSection* CS;
			Event* Event;
		};
	};

	template<typename T> struct ComPtr : ::winrt::com_ptr<T> 
	{
		using::winrt::com_ptr<T>::com_ptr;
		ComPtr(::winrt::com_ptr<T> p) : ::winrt::com_ptr<T>{p} {}
		FORCEINLINE T* get() const { return::winrt::com_ptr<T>::get(); }
		FORCEINLINE T* operator*() const { return::winrt::com_ptr<T>::get(); }
		FORCEINLINE operator T* const() const { return::winrt::com_ptr<T>::get(); }
		
		FORCEINLINE T** put() { return::winrt::com_ptr<T>::put(); }
		FORCEINLINE T** operator&() { return::winrt::com_ptr<T>::put(); }
	};
}

// Device child.
namespace twen 
{
	class Device;
	
	class DeviceChild 
	{
	public:
		FORCEINLINE DeviceChild(Device& device) : m_Device{ &device } {}

		FORCEINLINE auto& GetDevice() const 
		{
			assert(m_Device && "Device cannot be null.");
			return *m_Device;
		}
	private:
		Device *m_Device;
	};
}

// Node mask.
namespace twen
{
	class Device;

	struct NodeMask 
	{
		::UINT Creation;
		::UINT Visible;
	};

	struct SingleNodeObject
	{
		SingleNodeObject(NodeMask node) : NativeMask{ node.Creation } {}
		SingleNodeObject(::UINT mask)
			: NativeMask{ mask } {}

		bool InteractableWith(SingleNodeObject const&) const { return false; }
		const::UINT NativeMask;
	};

	struct MultiNodeObject 
	{
		MultiNodeObject(NodeMask nodeMask) 
			: VisibleMask{ nodeMask.Visible }, CreateMask{ nodeMask.Creation } {}
		MultiNodeObject(::UINT visibleMask, ::UINT createMask) 
			: VisibleMask{ visibleMask }, CreateMask{ createMask } {}

		// temporary.
		bool IntersectWith(MultiNodeObject const&) const { return false; }

		const::UINT VisibleMask;
		const::UINT CreateMask;
	};
}

namespace twen 
{
	inline constexpr struct 
	{
		inline static constexpr::UINT GetBitPerPixel(::DXGI_FORMAT format)
		{
			// From dxtk12
			//-------------------------------------------------------------------------------------
			// Returns bits-per-pixel for a given DXGI format, or 0 on failure
			//-------------------------------------------------------------------------------------
			switch (format)
			{
			case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
			case DXGI_FORMAT_R32G32B32A32_UINT:
			case DXGI_FORMAT_R32G32B32A32_SINT:
				return 128;

			case DXGI_FORMAT_R32G32B32_TYPELESS:
			case DXGI_FORMAT_R32G32B32_FLOAT:
			case DXGI_FORMAT_R32G32B32_UINT:
			case DXGI_FORMAT_R32G32B32_SINT:
				return 96;

			case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_UNORM:
			case DXGI_FORMAT_R16G16B16A16_UINT:
			case DXGI_FORMAT_R16G16B16A16_SNORM:
			case DXGI_FORMAT_R16G16B16A16_SINT:
			case DXGI_FORMAT_R32G32_TYPELESS:
			case DXGI_FORMAT_R32G32_FLOAT:
			case DXGI_FORMAT_R32G32_UINT:
			case DXGI_FORMAT_R32G32_SINT:
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			case DXGI_FORMAT_Y416:
			case DXGI_FORMAT_Y210:
			case DXGI_FORMAT_Y216:
				return 64;

			case DXGI_FORMAT_R10G10B10A2_TYPELESS:
			case DXGI_FORMAT_R10G10B10A2_UNORM:
			case DXGI_FORMAT_R10G10B10A2_UINT:
			case DXGI_FORMAT_R11G11B10_FLOAT:
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_UINT:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_R8G8B8A8_SINT:
			case DXGI_FORMAT_R16G16_TYPELESS:
			case DXGI_FORMAT_R16G16_FLOAT:
			case DXGI_FORMAT_R16G16_UNORM:
			case DXGI_FORMAT_R16G16_UINT:
			case DXGI_FORMAT_R16G16_SNORM:
			case DXGI_FORMAT_R16G16_SINT:
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_UINT:
			case DXGI_FORMAT_R32_SINT:
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			case DXGI_FORMAT_R8G8_B8G8_UNORM:
			case DXGI_FORMAT_G8R8_G8B8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			case DXGI_FORMAT_AYUV:
			case DXGI_FORMAT_Y410:
			case DXGI_FORMAT_YUY2:
				//case XBOX_DXGI_FORMAT_R10G10B10_7E3_A2_FLOAT:
				//case XBOX_DXGI_FORMAT_R10G10B10_6E4_A2_FLOAT:
				//case XBOX_DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM:
				return 32;

			case DXGI_FORMAT_P010:
			case DXGI_FORMAT_P016:
				//case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
				//case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
				//case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:
				//case WIN10_DXGI_FORMAT_V408:
				return 24;

			case DXGI_FORMAT_R8G8_TYPELESS:
			case DXGI_FORMAT_R8G8_UNORM:
			case DXGI_FORMAT_R8G8_UINT:
			case DXGI_FORMAT_R8G8_SNORM:
			case DXGI_FORMAT_R8G8_SINT:
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UNORM:
			case DXGI_FORMAT_R16_UINT:
			case DXGI_FORMAT_R16_SNORM:
			case DXGI_FORMAT_R16_SINT:
			case DXGI_FORMAT_B5G6R5_UNORM:
			case DXGI_FORMAT_B5G5R5A1_UNORM:
			case DXGI_FORMAT_A8P8:
			case DXGI_FORMAT_B4G4R4A4_UNORM:
				//case WIN10_DXGI_FORMAT_P208:
				//case WIN10_DXGI_FORMAT_V208:
				return 16;

			case DXGI_FORMAT_NV12:
			case DXGI_FORMAT_420_OPAQUE:
			case DXGI_FORMAT_NV11:
				return 12;

			case DXGI_FORMAT_R8_TYPELESS:
			case DXGI_FORMAT_R8_UNORM:
			case DXGI_FORMAT_R8_UINT:
			case DXGI_FORMAT_R8_SNORM:
			case DXGI_FORMAT_R8_SINT:
			case DXGI_FORMAT_A8_UNORM:
			case DXGI_FORMAT_AI44:
			case DXGI_FORMAT_IA44:
			case DXGI_FORMAT_P8:
				//case XBOX_DXGI_FORMAT_R4G4_UNORM:
				return 8;

			case DXGI_FORMAT_R1_UNORM:
				return 1;

			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
			case DXGI_FORMAT_BC4_SNORM:
				return 4;

			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
			case DXGI_FORMAT_BC5_SNORM:
			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
			case DXGI_FORMAT_BC6H_SF16:
			case DXGI_FORMAT_BC7_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				return 8;

			default:
				return 0;
			}
		}
		inline static constexpr::DXGI_FORMAT GetDepthFormat(::DXGI_FORMAT format)
		{
			// Copy from DirectX-Graphics-Sample.
			switch (format)
			{
				// 32-bit Z w/ Stencil
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
				return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

				// 24-bit Z
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
				return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

			default:
				return DXGI_FORMAT_UNKNOWN;
			}
		}
		inline static constexpr::DXGI_FORMAT GetDSVFormat(::DXGI_FORMAT format)
		{
			// Copy from DirectX-Graphics-Sample.
			switch (format)
			{
				// 32-bit Z w/ Stencil
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
				return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

				// No Stencil
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
				return DXGI_FORMAT_D32_FLOAT;

				// 24-bit Z
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
				return DXGI_FORMAT_D24_UNORM_S8_UINT;

				// 16-bit Z w/o Stencil
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UNORM:
				return DXGI_FORMAT_D16_UNORM;

			default:
				return DXGI_FORMAT_UNKNOWN;
			}
		}
		inline static constexpr::DXGI_FORMAT GetUAVFormat(::DXGI_FORMAT format)
		{
			// Copy from DirectX-Graphics-Sample.
			switch (format)
			{
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				return DXGI_FORMAT_R8G8B8A8_UNORM;

			case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
				return DXGI_FORMAT_B8G8R8A8_UNORM;

			case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
				return DXGI_FORMAT_B8G8R8X8_UNORM;

			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_R32_FLOAT:
				return DXGI_FORMAT_R32_FLOAT;

		#if IS_DEBUG
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			case DXGI_FORMAT_D16_UNORM:
				assert(!"Requested a UAV format for a depth stencil format.");

		#endif
			default:
				// return format.
				return DXGI_FORMAT_UNKNOWN;
			}
		}
		inline static constexpr::DXGI_FORMAT GetTypelessFormat(::DXGI_FORMAT format)
		{
			// Copy from DirectX-Graphics-Sample.
			switch (format)
			{
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				return DXGI_FORMAT_R8G8B8A8_TYPELESS;

			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
				return DXGI_FORMAT_B8G8R8A8_TYPELESS;

			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
				return DXGI_FORMAT_B8G8R8X8_TYPELESS;

				// 32-bit Z w/ Stencil
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
				return DXGI_FORMAT_R32G8X24_TYPELESS;

				// No Stencil
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
				return DXGI_FORMAT_R32_TYPELESS;

				// 24-bit Z
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
				return DXGI_FORMAT_R24G8_TYPELESS;

				// 16-bit Z w/o Stencil
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UNORM:
				return DXGI_FORMAT_R16_TYPELESS;

			default:
				return format;
			}
		}

		FORCEINLINE static constexpr::UINT GetSubresourceID(UINT mipSlice, UINT arraySlice, UINT planeSlice, UINT mipLevels, UINT arraySize)
		{
			return mipSlice + arraySlice * planeSlice + mipLevels * planeSlice * arraySize;
		}
		FORCEINLINE static::UINT16 GetMipLevel(::UINT width, ::UINT height)
		{
			return static_cast<UINT16>(::std::floor(::std::log2(width > height ? width : height))) + 1;
		}
	} Utils{};

	template<typename T>
	struct Pointer;

	struct PointerHasher 
	{
		template<typename T>
		inline::std::size_t operator()(Pointer<T> const& pointer) const
		{ return::std::hash<::std::size_t>(pointer.Offset); }
	};
}

// Concepts
namespace twen 
{
	template<typename Derived, typename Base>
	concept DerviedFrom = 
		::std::is_base_of_v<Base, Derived> 
		||::std::is_same_v<Derived, Base>;
}

namespace twen 
{
	template<typename T, typename...NT>
	struct ShareObject : public::std::enable_shared_from_this<T>
	{
		template<DerviedFrom<T> TargetT = T, typename...Args>
		static auto Create(NT...necessaryArgs, Args&&...args)
			noexcept(::std::is_nothrow_constructible_v<TargetT, NT..., Args...>)
			requires::std::constructible_from<TargetT, NT..., Args...>
		{ return::std::make_shared<TargetT>(necessaryArgs..., ::std::forward<Args>(args)...); }

		virtual ~ShareObject() = default;
	
		inline static::UINT Growing{0u};
		const::UINT ID{Growing++};
	};
}

namespace twen 
{
	struct LinkedObject 
	{
		LinkedObject* Prev;
		LinkedObject* Next;
	};
}