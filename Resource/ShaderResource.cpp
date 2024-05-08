#include "mpch.h"

#include "System\Device.h"

#include "Pipeline\Descriptor.h"
#include "Command\Context.h"
#include "Buffer.h"

#include "ShaderResource.h"

namespace twen 
{
	//void ShaderResource::CreateView(Pointer<DescriptorSet> const& position, ::UINT index) const
	//{ 
	//	GetDevice()->CreateShaderResourceView(*m_Resource, &m_ViewDesc, position[index]);
	//}

	void ShaderResource::Bind(DirectContext& context, ::UINT index) const
	{ 
		assert(index <= context.CSUHandles.Size && 
			"Temporary: CSUHandle size must be greater than index or equal to. That means you have to fill resource in order.");
		assert(context.BeginRecordingTable && "Should begin descriptor at first.");
		assert(!context.BeginSamplerTable && "Not allow record shader resource on sampler descriptor heap.");

		if(context.CSUHandles.Size == index) context.CSUHandles.AddView(weak_from_this());
		GetDevice()->CreateShaderResourceView(*m_Resource, &m_ViewDesc, context.CSUHandles[index]);
	}
	::D3D12_RESOURCE_BARRIER Texture::Transition(::D3D12_RESOURCE_STATES nextState, ::UINT index, ::D3D12_RESOURCE_BARRIER_FLAGS flags)
	{
		if (index == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		{
		#if D3D12_MODEL_DEBUG
			::D3D12_RESOURCE_STATES stateStandard{ m_States.front() };
			for (auto const& state : m_States)
				assert(stateStandard == state && "Should transition subresouce state first!");
		#endif	
			auto barrier = ResourceBarrier(*m_Resource, m_States.front(), nextState, index, flags);
			for (auto& state : m_States)
				state = nextState;
			return barrier;
		} else {
			assert(index < m_Footprints.size() && "Out of range.");
			auto stub = m_States.at(index);
			return ResourceBarrier(*m_Resource, stub, m_States.at(index) = nextState, index, flags);
		}
	}

	TextureCopy::TextureCopy(::std::shared_ptr<Texture> texture, AccessMode access)
		: m_Target{texture}, AccessibleBuffer{texture->GetDevice(), texture->ResourceSize, access} {}

	void TextureCopy::Write(ImageCopy::ImageSpan img, ::UINT index)
	{
		assert(!m_Target.expired()&& "Texture has been evicted. Copy is not an vaild copy.");

		auto texture{ m_Target.lock() };
		auto const& footprint{ texture->m_Footprints.at(index) };
		AccessibleBuffer::Write(ImageCopy{ footprint.Footprint.RowPitch, footprint.Offset, }, img);
	}
}