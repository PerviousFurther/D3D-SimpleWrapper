#include "mpch.h"

#if D3D12_MODEL_DEBUG
#pragma comment(lib, "dxguid.lib")
namespace twen::Debug
{
	struct Debug 
	{
		using GetInterfaceT = decltype(DXGIGetDebugInterface);

		Debug() : DllModule{ ::LoadLibraryW(L"dxgidebug.dll") },
			GetInterface{ (GetInterfaceT*)GetProcAddress(DllModule, "DXGIGetDebugInterface") }
		{
			assert(DllModule && "Debug module loading failure!");
			assert(GetInterface && "Debug proc cannot find.");

			GetInterface(IID_PPV_ARGS(DebugDXGI.Put()));
			assert(DebugDXGI && "Cannot get debug interface.");

			GetInterface(IID_PPV_ARGS(InfoQueue.Put()));
			assert(InfoQueue && "Cannot get debug interface.");

			InfoQueue->SetBreakOnSeverity(DXGI_DEBUG_D3D12, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
			InfoQueue->SetBreakOnSeverity(DXGI_DEBUG_D3D12, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
			InfoQueue->SetBreakOnSeverity(DXGI_DEBUG_D3D12, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
		}
		~Debug() 
		{
			DebugDXGI->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		}
		HMODULE DllModule;
		GetInterfaceT* GetInterface;

		ComPtr<::IDXGIDebug> DebugDXGI;
		ComPtr<::IDXGIInfoQueue> InfoQueue;
	} g_Debug{};
}

namespace twen
{
	using namespace Debug;

	static::std::string SeverityToString(DXGI_INFO_QUEUE_MESSAGE_SEVERITY severity)
	{
		switch (severity)
		{
		case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION:	return "[CORRUPTION]: ";
		case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR:		return "[ERROR]: ";
		case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING:		return "[WARNING]: ";
		case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO:			return "[INFO]: ";
		case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE:		return "[MESSAGE]: ";
		default:return "UNKNWON: ";
		}
	}

	::std::vector<::std::string> GetErrorInfos(::GUID const& guid)
	{
		auto nums = g_Debug.InfoQueue->GetNumStoredMessages(guid);
		if (!nums) return { "Unknown message." };

		::std::vector<::std::string> result{};
		for (auto i{ 0ull }; i < nums; i++)
		{
			SIZE_T size;
			g_Debug.InfoQueue->GetMessage(guid, i, nullptr, &size);

			::DXGI_INFO_QUEUE_MESSAGE* message = ::std::bit_cast<::DXGI_INFO_QUEUE_MESSAGE*>(new(::std::nothrow)::std::byte[size]);
			g_Debug.InfoQueue->GetMessage(guid, i, message, &size);

			result.emplace_back(SeverityToString(message->Severity) + message->pDescription);

			delete message;
		}
		g_Debug.InfoQueue->ClearStoredMessages(guid);
		return result;
	}

	void ReportLivingObject()
	{
		auto hr = g_Debug.DebugDXGI->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		assert(SUCCEEDED(hr) && "Cannot report living object.");
	}

	void SetFilters(::std::vector<::DXGI_INFO_QUEUE_FILTER> const& filters)
	{
		//g_Debug.InfoQueue->AddStorageFilterEntries(DXF);
	}
}
#endif // D3D12_MODEL_DEBUG

#include "System\Adapter.h"

namespace twen
{
	struct FactoryT
	{
		void Init(bool mediaEnable = false);

		::IDXGIFactory7* const operator->() const { return Factory.Get(); }

		ComPtr<::IDXGIFactory7> Factory;
		ComPtr<::IDXGIFactoryMedia> MediaFactory;

		::std::vector<ComPtr<::IDXGIAdapter4>> Adapters;

		::HANDLE AdapterChanged;
		::DWORD Cookie;

		::std::vector<Adapter*> m_Adapter;
	} g_Factory;

	void FactoryT::Init(bool mediaEnable)
	{
		::CreateDXGIFactory2(
#if D3D12_MODEL_DEBUG
			DXGI_CREATE_FACTORY_DEBUG
#else		
			0
#endif 
			, IID_PPV_ARGS(Factory.Put()));

		assert(Factory && "Factory initialize failure!");
		if (mediaEnable) {
			::CreateDXGIFactory2(
#if D3D12_MODEL_DEBUG
				DXGI_CREATE_FACTORY_DEBUG
#else		
				0
#endif 
				, IID_PPV_ARGS(MediaFactory.Put()));
			assert(MediaFactory && "MediaFactory Initialize failure.");
		}
		// future.
		//Factory->RegisterAdaptersChangedEvent(
		//	AdapterChanged = CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS), 
		//	&Cookie);
		//SetWait
	}

	void InitFactory(bool mediaFactory)
	{
		g_Factory.Init(mediaFactory);
	}

	::std::vector<ComPtr<::IDXGIAdapter4>> const& EnumAdapter()
	{
		ComPtr<::IDXGIAdapter1> temp;
		for (auto i{ 0u }; g_Factory->EnumAdapters1(i, temp.Put()) != DXGI_ERROR_NOT_FOUND; ++i)
			g_Factory.Adapters.emplace_back(temp.As<::IDXGIAdapter4>());

		return g_Factory.Adapters;
	}

	ComPtr<::IDXGISwapChain4> CreateSwapChain(::ID3D12CommandQueue* queue,
		::DXGI_SWAP_CHAIN_DESC1 const& desc, ::IDXGIOutput* restrictOutput)
	{
		ComPtr<::IDXGISwapChain1> result;
		auto hr = g_Factory->CreateSwapChainForComposition(queue, &desc, restrictOutput, result.Put());
		if (FAILED(hr)) 
		{
	#if D3D12_MODEL_DEBUG
			auto errors = GetErrorInfos();
			assert(!"Create swapchain failure.");
			return nullptr;
	#else
			throw::std::exception("Create swapchain failure.");
	#endif
		} else {

			return result.As<::IDXGISwapChain4>();
		}
	}
}

namespace twen
{	
	::std::wstring g_Prefix{L"Noname_"};

	void twen::SetNamePrefix(::std::wstring_view prefix)
	{
		g_Prefix = prefix;
	}
	::std::wstring GetName(::std::wstring_view name)
	{
		return g_Prefix + name.data();
	}
}