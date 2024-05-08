//*****
//* Export page.
//* blah blah...
#pragma once

#include "mpch.h"

#include "Misc\Debug.h"
// Misc
namespace twen
{
	void InitFactory(bool mediaFactory = false);

	::std::vector<ComPtr<::IDXGIAdapter4>> const& EnumAdapter();

	ComPtr<::IDXGISwapChain4> CreateSwapChain(::ID3D12CommandQueue* queue,
		::DXGI_SWAP_CHAIN_DESC1 const& desc, ::IDXGIOutput* restrictOutput = nullptr);
}

#include "System\Adapter.h"
#include "System\Device.h"

#include "Misc\Common.h"

#include "Resource\Buffer.h"
#include "Resource\ShaderResource.h"
#include "Resource\RenderTarget.h"

#include "Pipeline\Descriptor.h"
#include "Pipeline\Default.h"
#include "Pipeline\Compiler.h"
#include "Pipeline\Pipeline.h"

#include "Command\Query.h"

#include "Command\Context.h"

#ifndef DISABLE_CAMERA 
	#include "Camera.hpp"
#endif

namespace twen 
{
	template<typename ContextT>
	void TextureCopy::Copy(ContextT& cc)
	{
		static_assert(!::std::is_same_v<Context, ContextT>, "Common context should not use at here.");

		assert(!m_Target.expired() && "Target texture is expired.");

		auto texture{ m_Target.lock() };
		auto& resource{ m_Resource };
		for (auto i{ 0u }; auto const& footprint : texture->Footprints())
		{
			const::D3D12_TEXTURE_COPY_LOCATION thisCopy{
				*resource, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
				{.PlacedFootprint{ footprint }}
			};
			const::D3D12_TEXTURE_COPY_LOCATION otherCopy{
				*texture, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				{.SubresourceIndex{i++}}
			};
			if (Access == AccessWrite)
				cc.CommandList->CopyTextureRegion(&otherCopy, 0u, 0u, 0u, &thisCopy, nullptr);
			else
				cc.CommandList->CopyTextureRegion(&thisCopy, 0u, 0u, 0u, &otherCopy, nullptr);
		}
	}
}

#undef D3D12_MODEL_DEBUG
#undef DEBUG_OPERATION