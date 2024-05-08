#pragma once

//#include "Resource\Allocator.hpp"

namespace twen 
{
	class Resource;
	class Heap;

	class DescriptorSet;
	class CommandContextSet;

	class Queue : public::std::enable_shared_from_this<Queue>, public DeviceChild
	{
	public:
		using sort_t =::D3D12_COMMAND_LIST_TYPE;
	public:
		Queue(Device& device, ::D3D12_COMMAND_LIST_TYPE type, ::D3D12_COMMAND_QUEUE_PRIORITY priority,
			::D3D12_COMMAND_QUEUE_FLAGS flags =::D3D12_COMMAND_QUEUE_FLAG_NONE);

		//void AddPayloads(::std::unique_ptr<Context>&& context);
		void AddPayloads(::std::shared_ptr<CommandContextSet> contexts);

		//::ID3D12CommandAllocator* Allocator() const { return m_Allocator.get(); }
		void Wait(::UINT64 value = 0ull);
		::UINT64 Execute();

		const sort_t Type;

		operator::ID3D12CommandQueue*() const { return m_Handle.get(); }
	private:
		::std::vector<::ID3D12CommandList*> m_Payloads;

		ComPtr<::ID3D12Fence> m_Fence;
		::UINT64 m_LastTicket{ 0u };

		//ComPtr<::ID3D12CommandAllocator>	m_Allocator;
		ComPtr<::ID3D12CommandQueue>		m_Handle;
	};
	// Dangerous when occurs dead lock.
	FORCEINLINE void Queue::Wait(::UINT64 value)
	{
		//assert(m_LastTicket && "Queue was never been executed.");
		if (value)
		{
			if (m_Fence->GetCompletedValue() >= value)
				return;
			m_Fence->SetEventOnCompletion(value, nullptr);
		} else m_Fence->SetEventOnCompletion(m_LastTicket, nullptr);
	}

	class Device : public SingleNodeObject
	{
		friend class Adapter;
	public:
		static inline auto sm_DefualtHeapSize{ 1024 * 1024 * 10u };
		static constexpr auto FeatureLevel{::D3D_FEATURE_LEVEL_12_0};
	public:
		Device(NodeMask node, class Adapter& adapter);

		Device(Device const&) = delete;
		Device& operator=(Device const&) = delete;

		~Device();
	public:
		::ID3D12Device* operator->() const { return m_Device.get(); }

		FORCEINLINE::std::shared_ptr<DescriptorSet> 
			DescriptorSetOf(::D3D12_DESCRIPTOR_HEAP_TYPE type) const { return m_DescriptorSets.at(type); }

		//auto DescriptorManagerCSU()		const { return m_DescriptorManagers.at(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); }
		//auto DescriptorManagerRTV()		const { return m_DescriptorManagers.at(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); }
		//auto DescriptorManagerDSV()		const { return m_DescriptorManagers.at(D3D12_DESCRIPTOR_HEAP_TYPE_DSV); }
		//auto DescriptorManagerSampler()	const { return m_DescriptorManagers.at(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER); }
		
		FORCEINLINE auto QueueOf(::D3D12_COMMAND_LIST_TYPE type) { return m_MainQueues.at(type); }

		//auto QueueCopy() { return m_MainQueues.at(::D3D12_COMMAND_LIST_TYPE_COPY); }
		//auto QueueDirect() { return m_MainQueues.at(::D3D12_COMMAND_LIST_TYPE_DIRECT); }

		FORCEINLINE::std::shared_ptr<CommandContextSet> 
			CommandContextSetOf(::D3D12_COMMAND_LIST_TYPE type) const { return m_CommandSets.at(type); }

		template<::D3D12_FEATURE Feature>
		auto FeatureData(auto&&...args);

		template<typename T>
		bool IsFeatureSupport(T& featurData);

		::std::string Message();
	private:
		ComPtr<::ID3D12Device> m_Device;
		
		::std::unordered_map<::D3D12_HEAP_TYPE, 
			::std::shared_ptr<Heap>>					m_DefaultHeaps;
		::std::unordered_map<::D3D12_DESCRIPTOR_HEAP_TYPE,
			::std::shared_ptr<DescriptorSet>>			m_DescriptorSets;
		::std::unordered_map<::D3D12_COMMAND_LIST_TYPE, ::std::shared_ptr<Queue>> 
														m_MainQueues;
		::std::unordered_map<::D3D12_COMMAND_LIST_TYPE, ::std::shared_ptr<CommandContextSet>>
														m_CommandSets;

		//::D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS m_MultiSampleQualityLevels;

	#if D3D12_MODEL_DEBUG
		ComPtr<::ID3D12InfoQueue> m_InfoQueue;
	#endif
	};

	namespace inner 
	{
		template<::D3D12_FEATURE> struct FeatureEnumToFeatureData;
		template<typename T> struct FeatureDataToFeatureEnum;
	}

#define HELPER_DEFINE_(x) template<> struct inner::FeatureEnumToFeatureData<::D3D12_FEATURE_##x> {using type =::D3D12_FEATURE_DATA_##x;}
#define DEFINE_FDTFE(x) template<> struct inner::FeatureDataToFeatureEnum<::D3D12_FEATURE_DATA_##x> {static constexpr auto value =::D3D12_FEATURE_##x;}

	HELPER_DEFINE_(ARCHITECTURE);
	HELPER_DEFINE_(ARCHITECTURE1);
	HELPER_DEFINE_(D3D12_OPTIONS);
	HELPER_DEFINE_(D3D12_OPTIONS1);
	HELPER_DEFINE_(D3D12_OPTIONS2);

	DEFINE_FDTFE(MULTISAMPLE_QUALITY_LEVELS);
	//DEFINE_FDTFE();

#undef HELPER_DEFINE_

	template<::D3D12_FEATURE Feature>
	FORCEINLINE auto Device::FeatureData(auto&&...args)
	{
		using result_t = inner::FeatureEnumToFeatureData<Feature>::type;
		result_t result{::std::forward<decltype(args)>(args)...};
		auto hr = m_Device->CheckFeatureSupport(Feature, &result, sizeof(result_t));
		assert(SUCCEEDED(hr) && "Check feature support failure.");
		return result;
	}
	template<typename T>
	FORCEINLINE bool Device::IsFeatureSupport(T& featureData)
	{
		constexpr auto enum_value = inner::FeatureDataToFeatureEnum<T>::value;
		return SUCCEEDED(m_Device->CheckFeatureSupport(enum_value, &featureData, sizeof(T)));
	}
}