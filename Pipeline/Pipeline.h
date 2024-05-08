#pragma once

#include "Default.h"
#include "RootSignature.h"

namespace twen 
{
	template <typename T>
	class ShaderReflection;

	class VertexShader;
	class PixelShader;
	class GeometryShader;
	class HullShader;
	class DomainShader;
	class ComputeShader;

	class Pipeline : public ShareObject<Pipeline>, public DeviceChild, public MultiNodeObject
	{
	public:
		operator::ID3D12PipelineState* () const { return m_Handle.get(); }
	protected:
		Pipeline(Device& device, NodeMask node) : DeviceChild{device}, MultiNodeObject{node} {}

		ComPtr<::ID3DBlob> Cache() const
		{ 
			ComPtr<::ID3DBlob> result;
			m_Handle->GetCachedBlob(result.put());
			assert(result&&"Get pipeline state's cache failure.");
			return result; 
		}

	protected:
		ComPtr<::ID3D12PipelineState> m_Handle;
	};

	// reserved.
	//struct GraphicsPipelineDesc : ::D3D12_GRAPHICS_PIPELINE_STATE_DESC 
	//{
	//	GraphicsPipelineDesc(RootSignature& rootSignature) : RootSignature{rootSignature} { pRootSignature = rootSignature.Get(); }
	//
	//
	//	RootSignature& RootSignature;
	//};

	// TODO: Finish resource count initialize after shader reflection is complete.
	class GraphicsPipelineState : public Pipeline
	{
	public:
		GraphicsPipelineState(Device& device, ::std::shared_ptr<typename::twen::RootSignature> rootSignature, 
			::D3D12_GRAPHICS_PIPELINE_STATE_DESC const& desc)
			: Pipeline{ device, { device.NativeMask, desc.NodeMask } }
			, m_RootSignature{rootSignature}
			, RenderTargetCount{desc.NumRenderTargets}
			//, ResourceCount{ rootSignature->DescriptorsCount() } // TODO: 
		{ 
			assert(desc.pRootSignature ? *m_RootSignature == desc.pRootSignature : true && "Rootsignature mismatch.");
			device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_Handle.put())); 
			assert(m_Handle.get()&&"Create pipeline state failure.");
		}
		::std::shared_ptr<typename::twen::RootSignature> RootSignature() const { return m_RootSignature; }
		
		void EnableLib() { assert(!"Not implented."); }

		const::UINT ResourceCount{0u}; // TODO: enable when shader reflection enabled.
		const::UINT RenderTargetCount;
	private:
		::std::shared_ptr<typename::twen::RootSignature> m_RootSignature;
		ComPtr<::ID3D12PipelineLibrary> m_Lib;
	};
	class ComputePipelineState : public Pipeline
	{
	public:
		ComputePipelineState(Device& device, ::D3D12_COMPUTE_PIPELINE_STATE_DESC const& desc);

	};

	class PipelineManager
	{};
}