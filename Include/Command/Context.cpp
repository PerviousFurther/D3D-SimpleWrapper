#include "mpch.h"

#include "..\Resource\Allocator.hpp"

#include "..\System\Device.h"

#include "..\Pipeline\Descriptor.h"
#include "..\Resource\Buffer.h"
#include "..\Resource\ShaderResource.h"

#include "..\Pipeline\Default.h"
#include "..\Pipeline\RootSignature.h"
#include "..\Pipeline\Compiler.h"
#include "..\Pipeline\Pipeline.h"

#include "Query.h"

#include "Commands.hpp"
#include "Context.h"

namespace twen 
{
	// TODO: Maybe in future make descriptor set changable.
	DirectContext::DirectContext(CommandLists::Type* listToRecord, 
		::std::shared_ptr<GraphicsPipelineState> pso, 
		::std::vector<::std::shared_ptr<DescriptorSet>> const& sets)
		: Context{listToRecord, Type}
		, m_Roots{pso->EmbeddedRootSignature()->m_Roots}
		, m_StaticSampler{pso->EmbeddedRootSignature()->m_Samplers}
	{
		CommandList->SetPipelineState(*pso);

		::ID3D12DescriptorHeap* heaps[2]{};
		::UINT count{0u};

		for (auto& set : sets) 
		{
			switch (set->Type)
			{
			case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
			case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
				MODEL_ASSERT(count <= 2u, "Too many descriptor heap.");
				m_Handles.emplace(set->Type, set->Obtain(pso->EmbeddedRootSignature()->Requirement(set->Type)));
				heaps[count++] = *set;
				break;
			case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
				m_Handles.emplace(set->Type, set->Obtain(pso->RenderTargetCount));
				break;
			case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
				if(pso->DepthEnable || pso->StencilEnable) 
					m_Handles.emplace(set->Type, set->Obtain(1u));
				break;
			case D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES:assert(!"Unknown descriptor heap type.");
			default:break;
			}
		}
		CommandList->SetDescriptorHeaps(count, heaps);
		CommandList->SetGraphicsRootSignature(*pso->EmbeddedRootSignature());

		LinkPipeline(pso);
	}

	DirectContext::~DirectContext() 
	{
		for (auto&[_, handle] : m_Handles) handle.Discard();
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

	void DirectContext::Viewport(::D3D12_RECT scssior)
	{
		::D3D12_VIEWPORT viewport{ 0.0f, 0.0f, 
			static_cast<::FLOAT>(scssior.right - scssior.left), 
			static_cast<::FLOAT>(scssior.bottom - scssior.top), 0.0f, 1.0f};
		CommandList->RSSetViewports(1u, &viewport);
		CommandList->RSSetScissorRects(1u, &scssior);
	}
	void DirectContext::BeginRenderPass(const float rtvClearValue[4], ::D3D12_DEPTH_STENCIL_VALUE dsvClearValue, bool depthc, bool stencilc)
	{
		auto& rtvs{m_Handles.at(::D3D12_DESCRIPTOR_HEAP_TYPE_RTV)};
		auto const& dsvs{ m_Handles.contains(::D3D12_DESCRIPTOR_HEAP_TYPE_DSV) ? m_Handles.at(::D3D12_DESCRIPTOR_HEAP_TYPE_DSV) : Pointer<DescriptorSet>{} };

		::D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles(rtvs);
		::D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvs);
		CommandList->OMSetRenderTargets(m_NumRenderTarget, &rtvHandles, true, m_EnableDepth ? &dsvHandle : nullptr);
		if (rtvClearValue) 
		{
			for (auto i{ 0u }; i < m_NumRenderTarget; i++)
				CommandList->ClearRenderTargetView(rtvs[i], rtvClearValue, 0u, nullptr);
		}

		::D3D12_CLEAR_FLAGS flags{ static_cast<::D3D12_CLEAR_FLAGS>((depthc ? ::D3D12_CLEAR_FLAG_DEPTH : 0u) | (stencilc ? ::D3D12_CLEAR_FLAG_STENCIL : 0u )) };

		if(m_EnableDepth && (depthc || stencilc))
			CommandList->ClearDepthStencilView(dsvs, flags, dsvClearValue.Depth, dsvClearValue.Stencil, 0u, nullptr);
	}
	void DirectContext::Draw(::D3D_PRIMITIVE_TOPOLOGY topology, ::UINT count, ::UINT instanceCount)
	{
		CommandList->IASetPrimitiveTopology(topology);
		CommandList->DrawInstanced(count, instanceCount, 0u, 0u);
	}
	void DirectContext::EndRenderPass() 
	{

	}

	// Dumb method of link pipeline and root signature.
	// At first we try to bind descriptor table here (because in this place we can know all the linkage between pipeline and root signature).
	// however it not work.
	void DirectContext::LinkPipeline(::std::shared_ptr<GraphicsPipelineState> pso)
	{
		m_NumRenderTarget = pso->RenderTargetCount;
		m_EnableDepth = pso->DepthEnable;

		auto const& binds{ pso->m_Bindings };
		for (auto i{ 0u }; auto& root : m_Roots) 
		{
			if (root.Type == ::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) 
			{
				for (auto s{0u}; auto& range : root.Ranges)
				{
					unsigned short value{ range };
					if (binds.contains(value))
					{
						auto const& bind{ binds.at(value) };

						auto&& [pair, _]
						{
							m_Binds.emplace(bind.Name,
								Shaders::Input{ &range, bind.Name, bind.Type, 0u, i, s })
						};

						pair->second.Attach(range);

						s += root.Type == ::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE ? bind.BindCount : 0u;
					}
				}
			} else {
				unsigned short value{ root.Parameter };
				if (binds.contains(value)) 
				{
					auto const& bind{ binds.at(value) };

					auto&& [pair, _]
					{
						m_Binds.emplace(bind.Name,
							Shaders::Input{ &root.Parameter, bind.Name, bind.Type, 0u, i, 0u })
					};
					pair->second.Attach(root.Parameter);
				}
			}
			i++;
		}
		//for (auto i{ 0u }; auto & sampler : m_StaticSampler) 
		//{
		//	//if()
		//}
		assert(!m_Binds.empty() && "No descriptor was bound.");
	}
}
