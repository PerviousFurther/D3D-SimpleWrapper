#pragma once

#include <deque>
#include <mutex>

namespace twen
{
	namespace inner
	{
		template<typename R>
		concept Allocable = requires(R resource)
		{
			{ resource.Size } -> ::std::integral;
		} && 
		(requires(R resource) { resource.Alignment; } || requires{ R::Alignment; });

		template<typename Backing>
		struct Allocator;

		template<typename Backing>
		struct DemandAllocator;

		template<typename Backing>
		struct BuddyAllocator;

		template<typename Backing>
		class SegmentAllocator;
	}

	namespace Views 
	{
		struct Resource;

		template<typename View, typename Extension =::std::nullptr_t>
		class Bindable;


	}

	class DescriptorHeap;
	class QueryHeap;
	class Heap;

	class Resource;
	class CommittedResource;
	class PlacedResource;
	class ReservedResource;

	namespace Commands 
	{
		struct CopyContext;
		struct ComputeContext;
		struct DirectContext;
		struct BindingContext;
	}

	class CommandContext;

	class RootSignature;

	class ShaderManager;

	class Pipeline;
	class ComputePipelineState;
	class GraphicsPipelineState;
	class PipelineManager;

	struct ShaderResourceView;
	struct UnorderedAccessView;
	struct ConstantBufferView;
	struct SamplerView;

	struct RenderTargetView;
	struct DepthStencilView;

	struct VertexBufferView;
	struct IndexBufferView;

}

#include "Residency.h"

namespace twen
{
	class Queue : public inner::ShareObject<Queue>, public inner::DeviceChild
	{
	public:
		using sort_t = ::D3D12_COMMAND_LIST_TYPE;
	public:
		Queue(Device& device, ::D3D12_COMMAND_LIST_TYPE type,
			::UINT nodeMask,
			::D3D12_COMMAND_QUEUE_PRIORITY priority = ::D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			::D3D12_COMMAND_QUEUE_FLAGS flags = ::D3D12_COMMAND_QUEUE_FLAG_NONE);

		void AddPayloads(::std::vector<::ID3D12CommandList*>&& contexts) 
		{
			::std::swap(m_Payloads, contexts);
		}
		HRESULT Wait(::UINT64 value = 0ull);
		::UINT64 Execute();
		operator::ID3D12CommandQueue* () const { return m_Handle.Get(); }
	public:
		const sort_t Type;
	private:
		::std::vector<::ID3D12CommandList*> m_Payloads;

		ComPtr<::ID3D12Fence> m_Fence;
		::UINT64 m_LastTicket{ 0u };

		ComPtr<::ID3D12CommandQueue>		m_Handle;
	};
	// Dangerous when occurs dead lock.
	inline HRESULT Queue::Wait(::UINT64 value)
	{
		//assert(m_LastTicket && "Queue was never been executed.");
		if (value)
		{
			if (m_Fence->GetCompletedValue() >= value)
				return S_OK;
			return m_Fence->SetEventOnCompletion(value, nullptr);
		}
		else return m_Fence->SetEventOnCompletion(m_LastTicket, nullptr);
	}

}

namespace twen
{
	struct DeviceDesc 
	{
		::UINT Index : 8;
		::UINT NodeCount : 8;
		::D3D_FEATURE_LEVEL FeatureLevel : 16;
		
		::D3D12_RESOURCE_BINDING_TIER TierResourceBinding : 4;
		::D3D12_TILED_RESOURCES_TIER  TierTiledResource   : 4;
		::D3D12_CROSS_NODE_SHARING_TIER TierCrossNode     : 4;
		
		bool SupportStandardSwizzleRowLayout : 1 {false};

		bool ExpandComputeResourceState : 1 {false};
	};

	class Device 
		: public inner::SingleNodeObject
	{
		friend class Adapter;
		friend class CommandContext;

		friend void UpdateResidency(Device& device, Residency::Resident* resident);

	public:
		// Features testing to create device.
		// when no feature was supportted will throw an exception.
		TWEN_ISC::D3D_FEATURE_LEVEL Features[]
		{
			::D3D_FEATURE_LEVEL_12_2,
			::D3D_FEATURE_LEVEL_12_1,
			::D3D_FEATURE_LEVEL_12_0,
		#if defined(D3D12_ALLOW_FEATURE_11_X)
			::D3D_FEATURE_LEVEL_11_1,
			::D3D_FEATURE_LEVEL_11_0,
		#endif
		};

		// Set this value to change default heap size.
		// Was limited by 4mb to 128mb.
		static inline auto DefualtHeapSize{ 1024 * 1024 * 32u };

	public:

		Device(Adapter& parent, DeviceDesc const& desc, ComPtr<::ID3D12Device> device);

		Device(Device const&) = delete;

		~Device();

	public:
		// Obtain default descriptor manger.
		inline auto DescriptorManager() const { return m_DescriptorManager; }

		// Obtain default queue.
		inline auto Queue(::D3D12_COMMAND_LIST_TYPE type) { return m_MainQueues.at(type); }

		// Obtain default command context.
		inline auto GetCommandContext() const { MODEL_ASSERT(m_Context, "Must call begin frame at first."); return m_Context; }

		template<::D3D12_FEATURE Feature>
		auto FeatureData(auto&&...args);

		template<typename T>
		bool IsFeatureSupport(T& featurData);

		// Dynamic version of creating resource view.
		// DO NOT USE.
		//::std::shared_ptr<Views::Resource> Create(Views::Type type, ResourceOrder const& order);

		template<typename View>
		auto Create(ResourceOrder&& order, ::std::string_view name) requires 
			requires{ typename View::view; };

		::UINT AllVisibleNode() const { return (1 << Desc.NodeCount) - 1; }
		
		Adapter* SwitchToAdapter() const { return m_Adapter; }

		LARGE_INTEGER CreateTime() const;

		// Inner usage, do not invoke.
		void UpdateResidency(const Residency::Resident* resident);
		void Evict(const Residency::Resident* resident);

		::D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DeviceRemoveData() const noexcept;

		::std::string Message();

		::ID3D12Device* operator->() const { return m_Device; }
		operator::ID3D12Device* () const { return m_Device; }

		// Temporary, will be remove at soon.
		template<inner::congener<Residency::Resident> T, typename...Args>
		inline auto Create(Args&&...args)
			noexcept(::std::is_nothrow_constructible_v<T, Args...> || ::std::is_nothrow_constructible_v<T, Device&, Args...>) requires
		    ::std::constructible_from<T, Device&, Args...> // Must be constructable from args. 
		{ return m_ResidencyManager->NewResident<T>(*this, ::std::forward<Args>(args)...); }

		void Verify(HRESULT hr) 
		{
			switch (hr)
			{
			case S_OK:
				break;
			case DXGI_ERROR_DEVICE_REMOVED:
			{
				::D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumb{};
				m_DeviceRemoveData->GetAutoBreadcrumbsOutput(&breadcrumb);
				::D3D12_DRED_PAGE_FAULT_OUTPUT pageFault{};
				m_DeviceRemoveData->GetPageFaultAllocationOutput(&pageFault);

				MODEL_ASSERT(false, "Device was remove.");
			}break;
			case DXGI_ERROR_DEVICE_HUNG:
				break;
			case DXGI_ERROR_DEVICE_RESET:
				break;
			default:MODEL_ASSERT(false, "Error occured.");
				break;
			}
		}
	private:

		void InitializeDeviceContext();


	public:
		// Contains neccessary info that can help 
		// construct further operation.
		const DeviceDesc Desc;
	private:
		Adapter* m_Adapter;

		::ID3D12Device* m_Device;

	#if D3D12_MODEL_DEBUG
		ComPtr<::ID3D12InfoQueue> m_InfoQueue;
		ComPtr<::ID3D12DeviceRemovedExtendedData> m_DeviceRemoveData;
	#endif

		// Listen all the resident request.
		Residency::ResidencyManager* m_ResidencyManager;

		// Device allocator.
		friend struct ResourceAllocator;
		struct ResourceAllocator* m_Allocator;

		friend struct DescriptorManager;
		struct DescriptorManager* m_DescriptorManager;

		struct BindlessDescriptorManager;


		::std::unordered_map<::D3D12_COMMAND_LIST_TYPE, 
			::std::shared_ptr<class Queue>> m_MainQueues;

		CommandContext* m_Context;

		//::std::unordered_map<::D3D12_COMMAND_LIST_TYPE, 
		//	::std::shared_ptr<class CommandContextSet>> m_CommandSets;

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
	inline auto Device::FeatureData(auto&&...args)
	{
		using result_t = inner::FeatureEnumToFeatureData<Feature>::type;
		result_t result{::std::forward<decltype(args)>(args)...};
		auto hr = m_Device->CheckFeatureSupport(Feature, &result, sizeof(result_t));
		assert(SUCCEEDED(hr) && "Check feature support failure.");
		return result;
	}
	template<typename T>
	inline bool Device::IsFeatureSupport(T& featureData)
	{
		constexpr auto enum_value = inner::FeatureDataToFeatureEnum<T>::value;
		return SUCCEEDED(m_Device->CheckFeatureSupport(enum_value, &featureData, sizeof(T)));
	}

	::std::unique_ptr<Device> CreateDefaultDevice();
}