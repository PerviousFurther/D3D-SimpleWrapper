#include "mpch.h"

#include "System\Device.h"
#include "Pipeline\Descriptor.h"
#include "Command\Context.h"

#include "Buffer.h"

namespace twen 
{
	void VertexBuffer::Bind(DirectContext& context, ::UINT startSlot, ::UINT drop) const
	{ 
		assert(drop < m_Views.size() - startSlot&&"Current vertex buffer didnt have so much slots.");
		context.CommandList->IASetVertexBuffers(startSlot, static_cast<::UINT>(m_Views.size() - drop), m_Views.data()); 
		context.VerticesView = m_Views;
	}
	void IndexBuffer::Bind(DirectContext& context) const 
	{
		context.CommandList->IASetIndexBuffer(&m_View);
		context.IndicesView = m_View;
	}

	void ConstantBuffer<void>::Bind(DirectContext& context, ::UINT index) const
	{
		assert(m_Resource->BaseAddress() == m_View.BufferLocation && "Contant buffer location mismatch.");

		//auto bb = AccessibleBuffer::Create<ConstantBuffer<void>>(GetDevice(), MinimumAlignment, MinimumAlignment);
		//context.CommandList->CopyResource(*bb->m_Resource, *m_Resource);
		if(!context.BeginRecordingTable)
			context.CommandList->SetGraphicsRootConstantBufferView(index, m_View.BufferLocation);
		else {
			assert(index <= context.CSUHandles.Size &&
				"Temporary: CSUHandle size must be greater than index or equal to. That means you have to fill resource in order.");
			if(context.CSUHandles.Size == index) context.CSUHandles.AddView(weak_from_this());
			GetDevice()->CreateConstantBufferView(&m_View, context.CSUHandles[index]);
		}
	}
}
