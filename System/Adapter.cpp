#include "mpch.h"

#include "Device.h"
#include "Adapter.h"

#include "Misc\Debug.h"

namespace twen 
{
	Adapter::Adapter(ComPtr<::IDXGIAdapter4> adapter) : m_Adapter{ adapter }
	{
	}

}