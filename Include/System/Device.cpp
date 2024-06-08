#include "mpch.h"


#include "System\Residency.h"
#include "System\Adapter.h"
#include "System\Device.h"

#include "Resource\Resource.h"
#include "Resource\ResourceAllocator.hpp"
#include "Resource\Bindable.hpp"

#include "Pipeline\Default.h"
#include "Pipeline\Descriptor.h"
#include "Pipeline\Pipeline.h"

#include "Command\Query.h"
#include "Command\Context.h"


namespace twen::Debug 
{
	static::std::wstring DebugName{L"Unknown"};

	void Name(::std::wstring_view name) 
	{
		DebugName = name;
	}
	::std::wstring_view Name() 
	{
		return DebugName;
	}
}

namespace twen::Residency
{
	::ID3D12Pageable* Resident::Pageable() const
	{
		switch (ResidentType)
		{
		case resident::Resource:
			return *static_cast<const Resource*>(this);

		case resident::DescriptorHeap:
			return *static_cast<const DescriptorHeap*>(this);

		case resident::PipelineState:
			return *static_cast<const Pipeline*>(this);

		case resident::QueryHeap:
			return *static_cast<const QueryHeap*>(this);

		case resident::Heap:
			return *static_cast<const Heap*>(this);

		default:return nullptr;
		}
	}

}

namespace twen
{
	// Create device.
	Device* Adapter::CreateDevice()
	{
		MODEL_ASSERT(!m_Device, "Should not call when device has initialized.");

	#if D3D12_MODEL_DEBUG
		static::std::once_flag flag;
		auto call = []
			{
				ComPtr<::ID3D12Debug> debug;
				D3D12GetDebugInterface(IID_PPV_ARGS(debug.Put()));
				MODEL_ASSERT(debug, "GetDebuginterface failure.");
				debug->EnableDebugLayer();
			};
		::std::call_once(flag, call);
	#endif

		ComPtr<::ID3D12Device> device;
		if (Desc.MaxSupportFeature == 0xff) 
		{
			Desc.MaxSupportFeature = 0u;

			while(FAILED(::D3D12CreateDevice(m_Adapter3.Get(), Device::Features[Desc.MaxSupportFeature], IID_PPV_ARGS(device.Put()))))
			{
				Desc.MaxSupportFeature++;
				if (Desc.MaxSupportFeature > _countof(Device::Features)) 
					throw::std::exception("Adapter not support d3d12.");
			}
		}

		::D3D12_FEATURE_DATA_D3D12_OPTIONS option;
		device->CheckFeatureSupport(::D3D12_FEATURE_D3D12_OPTIONS, &option, sizeof(option));

		::D3D12_FEATURE_DATA_D3D12_OPTIONS1 option1;
		device->CheckFeatureSupport(::D3D12_FEATURE_D3D12_OPTIONS1, &option1, sizeof(option1));

		m_Device = ::std::make_unique<Device>
		(
			*this,
			DeviceDesc
			{
			  Desc.Index
			, device->GetNodeCount()
			, Device::Features[Desc.MaxSupportFeature]
			, option.ResourceBindingTier
			, option.TiledResourcesTier
			, option.CrossNodeSharingTier
			, static_cast<bool>(option.StandardSwizzle64KBSupported)
			, static_cast<bool>(option1.ExpandedComputeResourceStates)
			}
			, device
		);
		return m_Device.get();
	}
}

namespace twen
{

	Device::Device(Adapter& parent, DeviceDesc const& desc, ComPtr<ID3D12Device> device)
		: inner::SingleNodeObject{ 0u } // temporary.
		, m_Adapter{ &parent }
		, m_Device{ device.Get() }
		, Desc{ desc }
	#if D3D12_MODEL_DEBUG
		, m_InfoQueue{ device.As<::ID3D12InfoQueue>() }
		, m_DeviceRemoveData{ device.As<::ID3D12DeviceRemovedExtendedData>() }
	#endif
		, m_ResidencyManager{ new Residency::ResidencyManager{ *this } }
		, m_DescriptorManager{ new (struct DescriptorManager){ *this, 16, 16, 128, 64 } }
		, m_Allocator{ new ::twen::ResourceAllocator{*this, 1024u * 1024u, 65536u, 2u, ::D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT} }
		, m_Context{ new CommandContext{ *this } }
	{
		device->AddRef();

		MODEL_ASSERT(m_InfoQueue, "Not support info queue.");
		MODEL_ASSERT(m_DeviceRemoveData, "Not support device remove data.");

		::QueryPerformanceCounter(&m_ResidencyManager->m_InitTimeStamp);

		InitializeDeviceContext();
	}

	Device::~Device()
	{
		m_ResidencyManager->EvictAll();

		delete m_Context;
		delete m_DescriptorManager;

		m_Allocator->CleanUp();
		delete m_Allocator;
		delete m_ResidencyManager;

		m_Device->Release();
	}

	void Device::InitializeDeviceContext()
	{
		m_MainQueues.emplace(::D3D12_COMMAND_LIST_TYPE_DIRECT,
			Create<class Queue>(::D3D12_COMMAND_LIST_TYPE_DIRECT, AllVisibleNode()));
		m_MainQueues.emplace(::D3D12_COMMAND_LIST_TYPE_COPY,
			Create<class Queue>(::D3D12_COMMAND_LIST_TYPE_COPY, AllVisibleNode()));
		m_MainQueues.emplace(::D3D12_COMMAND_LIST_TYPE_COMPUTE,
			Create<class Queue>(::D3D12_COMMAND_LIST_TYPE_COMPUTE, AllVisibleNode()));
	}

	void Device::UpdateResidency(const Residency::Resident* resident)
	{
		MODEL_ASSERT(m_ResidencyManager->m_CurrentResidencySet, "Havent begun resident recording.");
		MODEL_ASSERT(resident, "Resident is empty.");

		m_ResidencyManager->m_CurrentResidencySet->ReferencedResident.emplace(resident);
	}

	void Device::Evict(const Residency::Resident* resident)
	{
		m_ResidencyManager->DeleteObject(resident);
	}

	::D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT Device::DeviceRemoveData() const noexcept
	{
		if (m_DeviceRemoveData)
		{
			::D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT output;
			m_DeviceRemoveData->GetAutoBreadcrumbsOutput(&output);
			return output;
		}
		else return {};
	}

	::std::string Device::Message()
	{
		::std::string result;
#if D3D12_MODEL_DEBUG
		auto num = m_InfoQueue->GetNumStoredMessages();
		if (num)
		{
			for (auto i{ 0u }; i < num; ++i)
			{
				SIZE_T size;
				m_InfoQueue->GetMessage(i, nullptr, &size);

				auto message = (::D3D12_MESSAGE*)GlobalAlloc(0, size);
				m_InfoQueue->GetMessage(i, message, &size);

				result += message->pDescription;
				result += '\n';

				GlobalFree(message);
			}
			m_InfoQueue->ClearStoredMessages();
		}
#endif 
		return result;
	}
}

namespace twen 
{
	Queue::Queue(Device& device, ::D3D12_COMMAND_LIST_TYPE type, 
		::UINT nodeMask,
		::D3D12_COMMAND_QUEUE_PRIORITY priority, 
		::D3D12_COMMAND_QUEUE_FLAGS flags)
		: inner::DeviceChild{device}, Type{type}
	{
		::D3D12_COMMAND_QUEUE_DESC desc{ type, priority, flags, nodeMask };
		device->CreateCommandQueue(&desc, IID_PPV_ARGS(m_Handle.Put()));
		assert(m_Handle && "Creating queue failure.");

		device->CreateFence(0ull, ::D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_Fence.Put()));
		assert(m_Fence && "Creating fence failure.");
	}

	::UINT64 Queue::Execute()
	{
		m_Handle->ExecuteCommandLists(static_cast<::UINT>(m_Payloads.size()), m_Payloads.data());

		for (auto const& cmdlist : m_Payloads)
			cmdlist->Release(); // these command list is not handle by com_ptr. So realease them.

		auto hr = m_Handle->Signal(m_Fence.Get(), ++m_LastTicket);
		if (FAILED(hr))
		{
		#if D3D12_MODEL_DEBUG
			auto message = GetDevice().Message();
			
			auto size = MultiByteToWideChar(CP_UTF8, NULL, message.data(), static_cast<int>(message.size()), nullptr, 0);
			::std::wstring wMessage(size, '\0');
			MultiByteToWideChar(CP_UTF8, NULL, message.data(), static_cast<int>(message.size()), wMessage.data(), static_cast<int>(message.size()));

			_wassert(wMessage.data(), __FILEW__, __LINE__);
		#else
			throw::std::exception("Inner error.");
		#endif
		}
		m_Payloads.clear();

		return m_LastTicket;
	}
}