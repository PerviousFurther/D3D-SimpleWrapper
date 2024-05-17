#pragma once

#include "Shader.hpp"
#include "RootSignature.h"

namespace twen 
{
	class RootSignature;
	struct VertexShader;
	struct PixelShader;
	struct GeometryShader;
	struct HullShader;
	struct DomainShader;
	struct ComputeShader;

	class Pipeline : public ShareObject<Pipeline>, public DeviceChild, public MultiNodeObject
	{
	public:
		operator::ID3D12PipelineState* () const { return m_Handle.Get(); }
	protected:
		Pipeline(Device& device, ::UINT visbleNode) : DeviceChild{device}, MultiNodeObject{ visbleNode, device.NativeMask} {}
	public:
		ComPtr<::ID3DBlob> Cache() const;
	protected:
		ComPtr<::ID3D12PipelineState> m_Handle;
	};
	inline ComPtr<::ID3DBlob> Pipeline::Cache() const
	{
		ComPtr<::ID3DBlob> result;
		m_Handle->GetCachedBlob(result.Put());
		assert(result && "Get pipeline state's cache failure.");
		return result;
	}

	// TODO: Seperate graphics pipeline desc into some pieces.
	class GraphicsPipelineState : public Pipeline
	{
		friend struct DirectContext;
	public:
		// Create pipeline with existing rootsignature.
		GraphicsPipelineState(Device& device, ::std::shared_ptr<RootSignature> rso, 
			::std::shared_ptr<VertexShader> shaderChain, ::D3D12_GRAPHICS_PIPELINE_STATE_DESC&& desc)
			: Pipeline{ device, desc.NodeMask }
			, DepthEnable(desc.DepthStencilState.DepthEnable)
			, StencilEnable(desc.DepthStencilState.StencilEnable)
			, RenderTargetCount{desc.NumRenderTargets}
		{
			assert(rso&&"Root signature is empty.");

			ShaderNode* node{shaderChain.get()};
			while (node)
			{
				for (auto& bind : node->BindingDescs)
					m_Bindings.emplace(bind, bind);

				node->Bytecode(desc), node = node->Next();
			}
			desc.pRootSignature = *rso;
			m_RootSignature = rso;

			device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_Handle.Put()));
			assert(m_Handle.Get() && "Create pipeline state failure.");
		}
		// Create pipeline with generate fitable rootsignature.
		// Warning: Using this method, all sampler is dynamic. TODO: offer static sampler table and find on it.
		GraphicsPipelineState(Device& device, 
			::std::shared_ptr<VertexShader> shaderChain, 
			::D3D12_GRAPHICS_PIPELINE_STATE_DESC&& desc)
			: Pipeline{ device, desc.NodeMask }
			, DepthEnable(desc.DepthStencilState.DepthEnable)
			, StencilEnable(desc.DepthStencilState.StencilEnable)
			, RenderTargetCount{desc.NumRenderTargets}
		{
			::D3D12_ROOT_SIGNATURE_FLAGS flags{};
			ShaderNode* node{shaderChain.get()};
			while (node) 
			{
				for (auto& bind : node->BindingDescs) 
					m_Bindings.emplace(bind, bind);

				flags |= node->Requirement;
				node->Bytecode(desc);
				node = node->Next();
			}

			RootSignatureBuilder builder{};
			for (auto const&[_,binds] : m_Bindings) 
			{
				auto type{ Utils.ShaderInputTypeToRootParameterType(binds.Type) };
				if (type ==::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE || binds.BindCount > 1u) 
				{
					auto rangeType{ Utils.ShaderInputTypeToDescriptorRangeType(binds.Type) };

					if (builder.NumDescriptors == -1)
						builder.BeginTable();
					else if (builder.Ranges.back().back().RangeType == ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ?
						rangeType !=::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER : rangeType ==::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
						builder.EndTable().BeginTable();

					builder.AddRange(rangeType, binds.BindCount, binds.BindPoint, binds.Space);
				} else {
					builder.AddRoot(type, binds.BindPoint, binds.Space);
				}
			}
			if (builder.NumDescriptors != -1) builder.EndTable();
			m_RootSignature = builder.Create(device, flags);
			desc.pRootSignature = *m_RootSignature;

			device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_Handle.Put()));
			assert(m_Handle && "Create pipeline state failure.");
		}

		::std::shared_ptr<RootSignature> EmbeddedRootSignature() const { return m_RootSignature; }
	public:
		const::UINT RenderTargetCount : 4;
		const bool DepthEnable		  : 1;
		const bool StencilEnable	  : 1;
	private:
		::std::shared_ptr<RootSignature> m_RootSignature;
		::std::unordered_map<::std::size_t, Shaders::Input> m_Bindings;
	};
	class ComputePipelineState : public Pipeline
	{
	public:
		ComputePipelineState(Device& device, ::D3D12_COMPUTE_PIPELINE_STATE_DESC const& desc);
		ComputePipelineState(::std::shared_ptr<ComputeShader> UNFINISHED);
	};
}	

namespace twen 
{
	class PipelineManager
	{};
}

namespace twen 
{
	inline void Shaders::Input::Attach(Descriptors::Range& range)&
	{
		//assert(range.Count>=BindCount && range.Register==BindPoint && "Cannot bind to specified range.");
		range.BindedInput = this;
		BindedRange = &range;
	}
	inline void Descriptors::Range::Attach(Shaders::Input& input)&
	{
		//assert(input.BindCount <= Count && Register == input.BindPoint && "Cannot bind input to this range.");
		input.BindedRange = this;
		BindedInput = &input;
	}
	inline void Shaders::Input::Detach()
	{
		assert(BindedRange && "Havent bound anything.");
		BindedRange->BindedInput = nullptr;
		BindedRange = nullptr;
	}
	inline void Descriptors::Range::Detach()
	{
		assert(BindedInput && "Havent bound anything.");
		BindedInput->BindedRange = nullptr;
		BindedInput = nullptr;
	}
	inline Shaders::Input::operator Descriptors::Range() const
	{
		return{ {}, BindCount, BindPoint, Space };
	}
	inline twen::Descriptors::Range::operator Shaders::Input() const
	{
		return{ nullptr, {}, {}, Count, Register, Space };
	}
}