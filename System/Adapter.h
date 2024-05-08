#pragma once

namespace twen 
{
	class Adapter 
	{
		friend class Device;
	public:
		Adapter(ComPtr<::IDXGIAdapter4> adapter);

		auto GetDesc() const;
		void OnAdapterChange(); // Temporary.
	private:
		ComPtr<::IDXGIAdapter4> m_Adapter;

		Device* m_Device;
	};
	inline auto Adapter::GetDesc() const
	{
		::DXGI_ADAPTER_DESC3 desc;
		m_Adapter->GetDesc3(&desc);
		return desc;
	}
}