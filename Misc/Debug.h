#pragma once

#if D3D12_MODEL_DEBUG
namespace twen 
{
	::std::vector<::std::string> GetErrorInfos(
		GUID const& guid =::DXGI_DEBUG_ALL);
	void ReportLivingObject();
	void SetFilters(::std::vector<::DXGI_INFO_QUEUE_FILTER> const& filters);

	void SetNamePrefix(::std::wstring_view prefix);
	::std::wstring GetName(::std::wstring_view name);
}
#define SET_NAME(object, name) object->SetName(GetName(name).data())
#else 
#define SET_NAME(...)
#endif