//*****
//* Export page.
//* blah blah...
#pragma once

#include "mpch.h"

#include "Misc\Debug.h"
// Misc
namespace twen
{
//#include <wrl\implements.h>

	void InitFactory(bool mediaFactory = false);

	::std::vector<ComPtr<::IDXGIAdapter4>> const& EnumAdapter();

	ComPtr<::IDXGISwapChain4> CreateSwapChain(::ID3D12CommandQueue* queue,
		::DXGI_SWAP_CHAIN_DESC1 const& desc, ::IDXGIOutput* restrictOutput = nullptr);
}

#include "System\Adapter.h"
#include "System\Device.h"

#include "Resource\Buffer.h"
#include "Resource\ShaderResource.h"
#include "Resource\RenderTarget.h"
#include "Resource\Sampler.h"

#include "Pipeline\Default.h"
#include "Pipeline\Descriptor.h"
#include "Pipeline\Default.h"
#include "Pipeline\Compiler.h"
#include "Pipeline\Pipeline.h"

#include "Command\Query.h"
#include "Command\Commands.hpp"
#include "Command\Context.h"

#ifndef DISABLE_CAMERA 
	#include "Camera.hpp"
#endif

#ifndef DIABLE_MESH_GENERATOR
	#include "MeshGenerator.hpp"
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
		for (auto i{ 0u }; auto const& footprint : texture->Footprints(0u))
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

	TextureCopy::TextureCopy(::std::shared_ptr<Texture> texture, AccessMode access)
		: m_Target{ texture }, AccessibleBuffer{ texture->GetDevice(), texture->ResourceSize, access } {}

	void TextureCopy::Write(ImageCopy::ImageSpan img, ::UINT index)
	{
		assert(!m_Target.expired() && "Texture has been evicted. Copy is not an vaild copy.");

		auto texture{ m_Target.lock() };
		auto const& footprint{ texture->m_Footprints.at(index) };
		AccessibleBuffer::Write(ImageCopy{ footprint.Footprint.RowPitch, footprint.Offset, footprint.Footprint.Height, footprint.Footprint.Width }, img);
	}
}

namespace twen 
{
	inline void RenderBuffer::Bind(DirectContext& context, ::UINT index) const
	{ GetDevice()->CreateRenderTargetView(*m_Resource, &m_View, context.ObtainHandle(::D3D12_DESCRIPTOR_HEAP_TYPE_RTV, index)); }

	inline void DepthStencil::Bind(DirectContext& context) const 
	{ GetDevice()->CreateDepthStencilView(*m_Resource, &m_View, context.ObtainHandle(::D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 0u)); }

	inline void RenderBuffer::Copy(DirectContext& context, ::std::shared_ptr<SwapchainBuffer> dst, ::UINT index) 
	{
		context.Transition(dst, ::D3D12_RESOURCE_STATE_RESOLVE_DEST);
		auto state = m_States.at(index);
		context.Transition(shared_from_this(), ::D3D12_RESOURCE_STATE_RESOLVE_SOURCE, index);
		context.FlushBarriars();

		assert(dst.use_count() && "Destination buffer was died.");
		context.CommandList->ResolveSubresource(dst->m_RenderBuffer.at(dst->m_Swapchain->GetCurrentBackBufferIndex()).Get(), 0u, *m_Resource, index, Desc.Format);

		context.Transition(shared_from_this(), state, index);
		context.Transition(dst, ::D3D12_RESOURCE_STATE_PRESENT);
		context.FlushBarriars();
	}
}

#undef D3D12_MODEL_DEBUG
#undef DEBUG_OPERATION
#undef BINDING_CHECK
#undef RANGE_CHECK