#include "mpch.h"

#include "System\Device.h"

#include "Pipeline\Default.h"
#include "Pipeline\Descriptor.h"
#include "Pipeline\RootSignature.h"

#include "Command\Context.h"

#include "Buffer.h"

namespace twen 
{
	void VertexBuffer::Bind(DirectContext& context, ::UINT startSlot, ::UINT drop) const
	{ 
		assert(drop < m_Views.size() - startSlot&&"Current vertex buffer didnt have so much slots.");
		context.CommandList->IASetVertexBuffers(startSlot, static_cast<::UINT>(m_Views.size() - drop), m_Views.data()); 

	}
	void IndexBuffer::Bind(DirectContext& context) const
	{
		context.CommandList->IASetIndexBuffer(&m_View);
	}
}
