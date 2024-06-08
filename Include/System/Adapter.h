#pragma once

namespace twen 
{
	struct AdapterDesc 
	{
		::UINT Index : 8;

		mutable::UINT8 MaxSupportFeature{0xffu};

		::SIZE_T SizeSharedMemory;
		::DXGI_QUERY_VIDEO_MEMORY_INFO LocalMemoryInfo;
		::DXGI_QUERY_VIDEO_MEMORY_INFO NonLocalMemoryInfo;

		::DXGI_COMPUTE_PREEMPTION_GRANULARITY ComputeGranularity;
		::DXGI_GRAPHICS_PREEMPTION_GRANULARITY GraphicsGranularity;

		static AdapterDesc InitFromAdapter3(ComPtr<::IDXGIAdapter3> adapter) 
		{
			AdapterDesc desc{};

			::DXGI_ADAPTER_DESC2 descA;
			adapter->GetDesc2(&descA);

			desc.GraphicsGranularity = descA.GraphicsPreemptionGranularity;
			desc.ComputeGranularity = descA.ComputePreemptionGranularity;
			desc.SizeSharedMemory = descA.SharedSystemMemory;

			adapter->QueryVideoMemoryInfo(desc.Index, ::DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &desc.LocalMemoryInfo);
			// If uma non local memory is zero, which is alias of video memory.
			adapter->QueryVideoMemoryInfo(desc.Index, ::DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &desc.NonLocalMemoryInfo);
			return desc;
		}
	};

	struct DeviceDesc;

	inline ComPtr<::IDXGIFactory6> Factory{nullptr};
	inline void InitFactory(bool debug = D3D12_MODEL_DEBUG) 
	{
		static::std::once_flag flag;
		auto initCall = [=]
		{
			if (FAILED(::CreateDXGIFactory2(debug ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(Factory.Put()))))
			{
			#if D3D12_MODEL_DEBUG
				MODEL_ASSERT(false, "Create factory failure.");
			#endif
				throw::std::runtime_error{"Create factory failure."};
			}
		};
		::std::call_once(flag, initCall);
	}

	class Adapter 
	{
		friend class Device;
	public:
		Adapter(AdapterDesc const& desc, ComPtr<::IDXGIAdapter3> adapter) 
			: Desc{ desc }
			, m_Adapter3{adapter}
		{}
		Adapter(AdapterDesc const& desc, ComPtr<::IDXGIAdapter4> adapter)
			: Desc{ desc }
			, m_Adapter4{ adapter }
		{}
		~Adapter() 
		{
			switch (sm_MaxSupportVersion)
			{
			case 4: m_Adapter4.~ComPtr();
				break;
			case 3: m_Adapter3.~ComPtr();
				break;
			}
		}
	public:
		Device* CreateDevice();
		Device* GetDevice() { MODEL_ASSERT(m_Device, "Should call create device before get."); return m_Device.get(); }

		bool IsUma() const { return !Desc.NonLocalMemoryInfo.Budget; }
	public:

		inline static::UINT sm_MaxSupportVersion{ 0u };

		const AdapterDesc Desc;

	private:

		union
		{
			ComPtr<::IDXGIAdapter4> m_Adapter4;
			ComPtr<::IDXGIAdapter3> m_Adapter3;
		};

		::std::unique_ptr<Device> m_Device;
	};

	inline Adapter* EnumAdapters(::UINT index)
	{
		static::std::unordered_map<::UINT, ::std::unique_ptr<Adapter>> Adapters{};

		if (Adapters.contains(index))
			return Adapters.at(index).get();

		InitFactory();

		ComPtr<::IDXGIAdapter1> adapter1;
		if (SUCCEEDED(Factory->EnumAdapters1(index, adapter1.Put())))
		{
			AdapterDesc desc{ index };
			if (ComPtr<::IDXGIAdapter4> adapter4 = adapter1.As<::IDXGIAdapter4>())
			{
				MODEL_ASSERT(!Adapter::sm_MaxSupportVersion || Adapter::sm_MaxSupportVersion == 4, "Adapter.");

				Adapter::sm_MaxSupportVersion = 4;

				auto&& [it, verify] {Adapters.emplace(index, ::std::make_unique<Adapter>(AdapterDesc::InitFromAdapter3(adapter4), adapter4)) };
				if (verify)
					return it->second.get();
				else
					throw::std::exception("No memory.");
			}
			else if (ComPtr<::IDXGIAdapter3> adapter3 = adapter1.As<::IDXGIAdapter4>()) {
				MODEL_ASSERT(!Adapter::sm_MaxSupportVersion || Adapter::sm_MaxSupportVersion == 3, "Adapter.");

				Adapter::sm_MaxSupportVersion = 3;

				auto&& [it, verify] {Adapters.emplace(index, ::std::make_unique<Adapter>(desc, adapter3)) };
				if (verify)
					return it->second.get();
				else
					throw::std::exception("No memory.");
			}
		}

		MODEL_ASSERT(false, "Cannot find adapter.");
		throw::std::runtime_error{ "Cannot find adapter." };
	}
}