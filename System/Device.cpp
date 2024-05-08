#include "mpch.h"

#include "System\Adapter.h"
#include "System\Device.h"

#include "Misc\Common.h"

#include "Resource\Buffer.h"
#include "Resource\ShaderResource.h"
#include "Pipeline\Descriptor.h"

#include "Pipeline\Default.h"
//#include "Pipeline\Compiler.h"
#include "Pipeline\Pipeline.h"

#include "Command\Context.h"

#if D3D12_MODEL_DEBUG
	::twen::ComPtr<::ID3D12Debug5> D3D12Debug;
#endif

namespace twen
{
	Device::Device(NodeMask node, Adapter& adapter) : SingleNodeObject{ node.Creation }
	{
		adapter.m_Device = this;

	#if D3D12_MODEL_DEBUG
		::D3D12GetDebugInterface(IID_PPV_ARGS(D3D12Debug.put()));
		assert(D3D12Debug.get() && "Initialize d3d12debug failure.");
		D3D12Debug->EnableDebugLayer();
	#endif
		// Create device.
		::D3D12CreateDevice(adapter.m_Adapter.get(), FeatureLevel, IID_PPV_ARGS(m_Device.put()));
		assert(m_Device && "Create device failure");

	#if D3D12_MODEL_DEBUG
		auto hr = m_Device.as(IID_PPV_ARGS(m_InfoQueue.put()));
		assert(m_InfoQueue.get() && "Initialize d3d12infoqueue failure.");
	#endif

		m_DefaultHeaps.emplace(::D3D12_HEAP_TYPE_DEFAULT, Heap::Create(*this, ::D3D12_HEAP_TYPE_DEFAULT, sm_DefualtHeapSize));
		m_DefaultHeaps.emplace(::D3D12_HEAP_TYPE_UPLOAD, Heap::Create(*this, ::D3D12_HEAP_TYPE_UPLOAD, sm_DefualtHeapSize));
		m_DefaultHeaps.emplace(::D3D12_HEAP_TYPE_READBACK, Heap::Create(*this, ::D3D12_HEAP_TYPE_UPLOAD, sm_DefualtHeapSize));

		m_DescriptorSets.emplace(::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
			DescriptorSet::Create(*this, ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
				128u, ::D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE));
		m_DescriptorSets.emplace(::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			DescriptorSet::Create(*this, ::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
				128u, ::D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE));
		m_DescriptorSets.emplace(::D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			DescriptorSet::Create(*this, ::D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 12u));
		m_DescriptorSets.emplace(::D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			DescriptorSet::Create(*this, ::D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 6u));

		m_MainQueues.emplace(::D3D12_COMMAND_LIST_TYPE_DIRECT, 
			::std::make_shared<Queue>(*this, ::D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL));
		m_MainQueues.emplace(::D3D12_COMMAND_LIST_TYPE_COPY, 
			::std::make_shared<Queue>(*this, ::D3D12_COMMAND_LIST_TYPE_COPY, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL));

		m_CommandSets.emplace(::D3D12_COMMAND_LIST_TYPE_DIRECT,
			::std::make_shared<CommandContextSet>(*this, ::D3D12_COMMAND_LIST_TYPE_DIRECT));
		m_CommandSets.emplace(::D3D12_COMMAND_LIST_TYPE_COPY,
			::std::make_shared<CommandContextSet>(*this, ::D3D12_COMMAND_LIST_TYPE_COPY));
		m_CommandSets.emplace(::D3D12_COMMAND_LIST_TYPE_COMPUTE,
			::std::make_shared<CommandContextSet>(*this, ::D3D12_COMMAND_LIST_TYPE_COMPUTE));
	}

	Device::~Device()
	{
		ReportLivingObject();
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

	Queue::Queue(Device& device, ::D3D12_COMMAND_LIST_TYPE type, ::D3D12_COMMAND_QUEUE_PRIORITY priority, 
		::D3D12_COMMAND_QUEUE_FLAGS flags)
		: DeviceChild{device}, Type{type}
	{
		::D3D12_COMMAND_QUEUE_DESC desc{ type, priority, flags, device.NativeMask };
		device->CreateCommandQueue(&desc, IID_PPV_ARGS(m_Handle.put()));
		assert(m_Handle && "Creating queue failure.");

		device->CreateFence(0ull, ::D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_Fence.put()));
		assert(m_Fence && "Creating fence failure.");

		//device->CreateCommandAllocator(type, IID_PPV_ARGS(m_Allocator.put()));
	}

	void Queue::AddPayloads(::std::shared_ptr<CommandContextSet> contexts)
	{ 
		assert(contexts->Type == Type && "Command context must in same type with queue.");
		assert(contexts->m_ExecutionQueue.expired() && "Command context is already on executing.");

		m_Payloads.append_range(::std::move(contexts->m_Payloads));
		contexts->m_Payloads.clear();

		contexts->m_LastTicket = m_LastTicket++; // i dont know it's safe or not. Ideally there is no any extra execution.
		contexts->m_ExecutionQueue = weak_from_this();
		contexts->Submitted = true;
	}

	::UINT64 Queue::Execute()
	{
		m_Handle->ExecuteCommandLists(static_cast<::UINT>(m_Payloads.size()), m_Payloads.data());

		for (auto const& cmdlist : m_Payloads)
			cmdlist->Release(); // these command list is not handle by com_ptr. So realease them.

		auto hr = m_Handle->Signal(m_Fence.get(), ++m_LastTicket);
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