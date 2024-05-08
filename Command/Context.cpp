#include "mpch.h"

#include "Resource\Allocator.hpp"

#include "System\Device.h"
#include "Pipeline\Descriptor.h"
#include "Resource\Buffer.h"
#include "Resource\ShaderResource.h"

#include "Pipeline\Pipeline.h"

#include "Query.h"

//#include "SyncPoint.h"
#include "Context.h"

//namespace twen 
//{
//	bool Internment::Close(::twen::Queue* queue)
//	{
//		ScopeLock _{ CriticalSection };
//		assert(!"Not implmented.");
//		queue->Execute();
//		return true;
//	}
//
//}

namespace twen 
{
	//void Context::Wait(::std::shared_ptr<Queue> queue, ::UINT64 value)
	//{
	//	
	//}

	DirectContext::DirectContext(CommandList::Type* listToRecord,
		::std::shared_ptr<GraphicsPipelineState> pso, ::std::shared_ptr<::twen::RootSignature> rso,
		::std::shared_ptr<DescriptorSet> rtvSet, ::std::shared_ptr<DescriptorSet> dsvSet, 
		::std::shared_ptr<DescriptorSet> csuSet, ::std::shared_ptr<DescriptorSet> samplerSet)
		: Context{ listToRecord, Type }
		, GraphicsState{ pso }
		, RTVHandles{ rtvSet->Obtain(pso->RenderTargetCount) }
		, CSUHandles{ csuSet->Obtain(rso->DescriptorsCount()) }
		, DSVHandle{ dsvSet->Obtain(1u) }
		, SamplerHandles{ samplerSet->Obtain(rso->SamplerCount()) }
		, BindedRootSignature{rso}
	{
		assert(RTVHandles.Capability && CSUHandles.Capability && DSVHandle.Capability && 
			(rso->SamplerCount() ? SamplerHandles.Capability : true) && "Descriptor set corrupted.");
		CommandList->SetPipelineState(*pso);
		::ID3D12DescriptorHeap* heaps[2]{*csuSet, samplerSet ? *samplerSet : nullptr};
		CommandList->SetDescriptorHeaps(samplerSet ? 2u : 1u, heaps);
		CommandList->SetGraphicsRootSignature(*rso);
	}

	DirectContext::~DirectContext() 
	{
		SamplerHandles.Discard();
		CSUHandles.Discard();
		DSVHandle.Discard(); 
		RTVHandles.Discard(); 
	}

	void Context::BeginQuery(Pointer<QuerySet> const& query)
	{
		CommandList->BeginQuery(*query.BackingSet.lock(), query.Type, query.Offset);
	}
	void Context::EndQuery(Pointer<QuerySet> const& query)
	{
		CommandList->EndQuery(*query.BackingSet.lock(), query.Type, query.Offset);
		assert(!query.OutDestination.expired() && "Query resource is empty, which is not allowed.");
		CommandList->ResolveQueryData(*query.BackingSet.lock(), query.Type, query.Offset, query.Capability, **query.OutDestination.lock(), 0u);
	}

	void DirectContext::Viewport(::D3D12_RECT viewport)
	{
		::D3D12_VIEWPORT viewport_{ 0.0f, 0.0f, 
			static_cast<::FLOAT>(viewport.right - viewport.left), 
			static_cast<::FLOAT>(viewport.bottom - viewport.top), 0.0f, 1.0f};
		CommandList->RSSetViewports(1u, &viewport_);
		CommandList->RSSetScissorRects(1u, &viewport);
		ViewPortWasSet = true;
	}
	void DirectContext::BeginRenderPass(const float rtvClearValue[4], ::D3D12_DEPTH_STENCIL_VALUE dsvClearValue, bool depthc, bool stencilc)
	{
		::D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles(RTVHandles);
		::D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle(DSVHandle);
		CommandList->OMSetRenderTargets(RTVHandles.Size, &rtvHandles, true, DSVHandle.Size ? &dsvHandle : nullptr);
		if (rtvClearValue) 
		{
			assert(RenderTargetWasSet && "Render target was not set.");
			for (auto i{ 0u }; i < RTVHandles.Size; i++)
				CommandList->ClearRenderTargetView(RTVHandles[i], rtvClearValue, 0u, nullptr);
		}

		::D3D12_CLEAR_FLAGS flags{ static_cast<::D3D12_CLEAR_FLAGS>((depthc ? ::D3D12_CLEAR_FLAG_DEPTH : 0u) | (depthc ? ::D3D12_CLEAR_FLAG_STENCIL : 0u )) };
		if((depthc || stencilc) && DSVHandle.Size) 
			CommandList->ClearDepthStencilView(DSVHandle, flags, dsvClearValue.Depth, dsvClearValue.Stencil, 0u, nullptr);
	}
	void DirectContext::BeginDescriptorTable(bool sampler)
	{
		assert(!BeginRecordingTable && "Dont worried, filling sampler table behind finished filling other table is ok.");

		if (sampler) TableIndex = SamplerHandles.Size;
		else TableIndex = CSUHandles.Size;
		BeginSamplerTable = sampler;
		BeginRecordingTable = true; // Probably remove this in future.
	}
	void DirectContext::EndDescriptorTable(::UINT index, bool sampler) 
	{
		if (sampler) CommandList->SetGraphicsRootDescriptorTable(index, SamplerHandles(TableIndex));
		else CommandList->SetGraphicsRootDescriptorTable(index, CSUHandles(TableIndex));

		BeginSamplerTable = false;
		BeginRecordingTable = false;
		TableIndex = 0u;
	}
	//void DirectContext::SetRootsignature(::std::shared_ptr<RootSignature> rootsignature) 
	//{

	//}
	//void DirectContext::RenderTargetSingle(ComPtr<ID3D12Resource> renderTarget, const float(&rtvClearValue)[4])
	//{
	//	::D3D12_CPU_DESCRIPTOR_HANDLE handles[]{RTVHandles};
	//	CommandList->OMSetRenderTargets(1u, handles, true, nullptr);
	//	CommandList->ClearRenderTargetView(handles[0], rtvClearValue, 0u, nullptr);
	//}

	void DirectContext::Draw(::D3D_PRIMITIVE_TOPOLOGY topology, ::UINT count, ::UINT instanceCount)
	{
		assert(RenderTargetWasSet && "Render target must be set.");
		assert(ViewPortWasSet && "Viewport was not set.");
		assert(!VerticesView.empty() && "Vertex buffer was not set.");

		CommandList->IASetPrimitiveTopology(topology);
		if (IndicesView.BufferLocation)
			CommandList->DrawIndexedInstanced(count, instanceCount, 0u, 0u, 0u);
		else
			CommandList->DrawInstanced(count, instanceCount, 0u, 0u);
	}
	void DirectContext::EndRenderPass() 
	{

	}
	//void CopyContext::CopyTexture(::std::shared_ptr<TextureCopy> pcopyBuffer) 
	//{
	//	auto copyBuffer = *pcopyBuffer;
	//	assert(!copyBuffer.m_Target.expired() && "Target texture is expired.");

	//	auto texture{ copyBuffer.m_Target.lock() };
	//	auto resource{ copyBuffer.m_Resource };
	//	for (auto i{ 0u }; auto const& footprint : texture->GetFootprints(0))
	//	{
	//		const::D3D12_TEXTURE_COPY_LOCATION thisCopy{
	//			*resource, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
	//			{.PlacedFootprint{ footprint }}
	//		};
	//		const::D3D12_TEXTURE_COPY_LOCATION otherCopy{
	//			*texture, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
	//			{.SubresourceIndex{i++}}
	//		};
	//		// TODO: not fit with resolve subresource.
	//		if (copyBuffer.Access == copyBuffer.AccessWrite)
	//			CommandList->CopyTextureRegion(&otherCopy, 0u, 0u, 0u, &thisCopy, nullptr);
	//		else 
	//			CommandList->CopyTextureRegion(&thisCopy, 0u, 0u, 0u, &otherCopy, nullptr);
	//	}
	//}

}
