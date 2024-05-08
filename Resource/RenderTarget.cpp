#include "mpch.h"

#include "System\Device.h"

#include "Pipeline\Descriptor.h"
#include "Command\Context.h"

#include "RenderTarget.h"

namespace twen
{
	::D3D12_RESOURCE_BARRIER RenderBuffer::Transition(::D3D12_RESOURCE_STATES newState, ::UINT index, ::D3D12_RESOURCE_BARRIER_FLAGS flags)
	{
		::D3D12_RESOURCE_STATES oldState{m_States.front()};
		if (index == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) 
		{
			for (auto& state : m_States) 
			{
				assert(oldState == state && "Subresource state is mismatch. TODO: resource state didnt be tracked.");
				state = newState;
			}
		} else {
			assert(index < m_States.size() && "Out of range.");
			auto old = m_States.at(index);
			m_States.at(index) = newState;
		}
		return ResourceBarrier(*m_Resource, oldState, newState, index, flags);
	}
	void RenderBuffer::Bind(DirectContext& context, ::UINT index) const
	{
		assert(index <= context.RTVHandles.Size &&
			"Temporary: CSUHandle size must be greater than index or equal to. That means you have to fill resource in order.");

		if (context.RTVHandles.Size == index) context.RTVHandles.AddView(weak_from_this());
		GetDevice()->CreateRenderTargetView(*m_Resource, &m_View, context.RTVHandles[index]);
		context.RenderTargetWasSet = true;
	}
	void RenderBuffer::Copy(DirectContext& context, ::std::shared_ptr<SwapchainBuffer> dst, ::UINT index)
	{
		context.Transition(dst, ::D3D12_RESOURCE_STATE_RESOLVE_DEST);
		auto state = m_States.at(index);
		context.Transition(shared_from_this(), ::D3D12_RESOURCE_STATE_RESOLVE_SOURCE, index);
		context.FlushBarriars();

		assert(dst.use_count() && "Destination buffer was died.");
		context.CommandList->ResolveSubresource(dst->m_Handle, 0u, *m_Resource, index, Desc.Format);

		context.Transition(shared_from_this(), state, index);
		context.Transition(dst, ::D3D12_RESOURCE_STATE_PRESENT);
		context.FlushBarriars();
	}

	void DepthStencil::Bind(DirectContext& context) const
	{
		if (!context.DSVHandle.Size) context.DSVHandle.AddView(weak_from_this());
		GetDevice()->CreateDepthStencilView(*m_Handle, &m_Desc, context.DSVHandle);
	}
}
