#pragma once

#include "Shader.hpp"
#include "RootSignature.h"

TWEN_EXPORT namespace twen
{
	template<>
	inline::std::shared_ptr<RootSignature> CreateRootSignature(Device& device,
		::D3D12_ROOT_SIGNATURE_FLAGS flags, ::std::vector<Shaders::Input> const& inputs)
	{
		RootSignatureBuilder builder{};
		for (auto const& binds : inputs)
		{
			auto type{ Utils::ToRootParameterType(binds.Type) };
			if (type == ::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE || binds.BindCount > 1u)
			{
				auto rangeType{ Utils::ToDescriptorRangeType(binds.Type) };

				if (builder.NumDescriptors == -1)
					builder.BeginTable();
				else if (builder.Ranges.back().back().RangeType == ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ?
					rangeType != ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER : rangeType == ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
					builder.EndTable().BeginTable();

				builder.AddRange(rangeType, binds.BindCount, binds.BindPoint, binds.Space);
			}
			else builder.AddRoot(type, binds.BindPoint, binds.Space);
		}
		if (builder.NumDescriptors != -1) builder.EndTable();

		return builder.Create(device, flags);
	}
}

namespace twen::inner
{
	struct BlendDesc
	{
		::D3D12_BLEND DestBlend;
		::D3D12_BLEND SrcBlend;
		::D3D12_BLEND_OP BlendOp;
		::D3D12_BLEND SrcBlendAlpha;
		::D3D12_BLEND DestBlendAlpha;
		::D3D12_BLEND_OP BlendOpAlpha;
	};

	struct DepthBiasState
	{
		::INT DepthBias{ 0 };
		::FLOAT DepthBiasClamp{ 0.0f };
		::FLOAT SlopeScaledDepthBias{ 0.0f };
	};

	struct Rasterizer
	{
		::D3D12_FILL_MODE FillMode{::D3D12_FILL_MODE_SOLID};
		::D3D12_CULL_MODE CullMode{::D3D12_CULL_MODE_NONE};
		::BOOL FrontCounterClockwise;
		::BOOL DepthClipEnable;
		::BOOL MultisampleEnable;
		::BOOL AntialiasedLineEnable;
		::UINT ForcedSampleCount;
		::D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
	};

	class GraphicsPipelineBuilder : private::D3D12_GRAPHICS_PIPELINE_STATE_DESC
	{
		using base_t = ::D3D12_GRAPHICS_PIPELINE_STATE_DESC;

		friend class GraphicsPipelineState;
	public:
		GraphicsPipelineBuilder(RootSignature* rootsignature)
			: base_t
			{
			.pRootSignature{*rootsignature},
			.BlendState{.RenderTarget{{.RenderTargetWriteMask{0xff}}}},
			.SampleMask{0xffffffff},
			.RasterizerState
				{ 
				.FillMode{::D3D12_FILL_MODE_SOLID},
				.CullMode{::D3D12_CULL_MODE_NONE}, 
				},
			.PrimitiveTopologyType{::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE},
			.SampleDesc{1u},
			}
			, EnableStreamOutput{ static_cast<bool>(rootsignature->Flags & ::D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT) }
			, EnableInputAssembly{ static_cast<bool>(rootsignature->Flags & ::D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT) }
		{}

		GraphicsPipelineBuilder(::ID3DBlob* cache)
			: base_t{.CachedPSO{cache->GetBufferPointer(), cache->GetBufferSize()}}
		{}

		auto&& Shaders(ShaderNode* shaders) 
		{
			base_t::VS = {};
			base_t::PS = {};
			base_t::DS = {};
			base_t::GS = {};
			base_t::HS = {};

			while (shaders)
			{
				shaders->Bytecode(*this);
				shaders = shaders->Next();
			}
			return *this;
		}
		auto&& AlphaBlendState(BOOL alphaToCoverageEnable, BOOL independentBlendEnable)
		{
			BlendState.AlphaToCoverageEnable = alphaToCoverageEnable;
			BlendState.IndependentBlendEnable = independentBlendEnable;
			return *this;
		}
		auto&& RenderTargetBlend(::UINT index, BlendDesc const& desc)
		{
			MODEL_ASSERT(index < 8, "Out of range.");
			NumRenderTargets = (::std::max)(NumRenderTargets, index + 1);

			BlendState.RenderTarget[index].LogicOpEnable = false;
			BlendState.RenderTarget[index] =
			{
				.BlendEnable{true},
				.SrcBlend{desc.SrcBlend},
				.DestBlend{desc.SrcBlendAlpha},
				.BlendOp{desc.BlendOp},
				.SrcBlendAlpha{desc.SrcBlendAlpha},
				.DestBlendAlpha{desc.DestBlendAlpha},
				.BlendOpAlpha{desc.BlendOpAlpha},
			};
			return *this;
		}
		auto&& RenderTargetBlend(::UINT index, ::D3D12_LOGIC_OP logicOp)
		{
			MODEL_ASSERT(index < 8, "Out of range.");
			NumRenderTargets = (::std::max)(NumRenderTargets, index + 1);

			BlendState.RenderTarget[index].BlendEnable = false;
			BlendState.RenderTarget[index] =
			{
				.LogicOpEnable{true},
				.LogicOp{logicOp}
			};
			return *this;
		}
		auto&& RenderTargetState(::UINT index, ::D3D12_COLOR_WRITE_ENABLE color, ::DXGI_FORMAT format = ::DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			MODEL_ASSERT(index < 8, "Out of range.");
			NumRenderTargets = (::std::max)(NumRenderTargets, index + 1);

			BlendState.RenderTarget[index].RenderTargetWriteMask = static_cast<::UINT8>(color);
			base_t::RTVFormats[index] = format;
			return *this;
		}
		auto&& SampleState(::DXGI_SAMPLE_DESC desc, ::UINT mask = UINT_MAX)
		{
			MODEL_ASSERT(desc.Count >= 1u, "Sample count must greater than or equal with 1u");
			base_t::SampleDesc = desc;
			base_t::SampleMask = mask;
			return *this;
		}
		auto&& RasterizerState(DepthBiasState const& depthBias)
		{
			base_t::RasterizerState.DepthBias = depthBias.DepthBias;
			base_t::RasterizerState.DepthBiasClamp = depthBias.DepthBiasClamp;
			base_t::RasterizerState.SlopeScaledDepthBias = depthBias.SlopeScaledDepthBias;

			return *this;
		}
		auto&& RasterizerState(Rasterizer const& rasterizer)
		{
			base_t::RasterizerState.
				FillMode = rasterizer.FillMode;
			base_t::RasterizerState.
				CullMode = rasterizer.CullMode;
			base_t::RasterizerState.
				FrontCounterClockwise = rasterizer.FrontCounterClockwise;
			base_t::RasterizerState.
				DepthClipEnable = rasterizer.DepthClipEnable;
			base_t::RasterizerState.
				MultisampleEnable = rasterizer.MultisampleEnable;
			base_t::RasterizerState.
				AntialiasedLineEnable = rasterizer.AntialiasedLineEnable;
			base_t::RasterizerState.
				ForcedSampleCount = rasterizer.ForcedSampleCount;
			base_t::RasterizerState.
				ConservativeRaster = rasterizer.ConservativeRaster;

			return *this;
		}
		auto&& DepthState(bool enableWrite = true, ::D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_NEVER, ::DXGI_FORMAT format = ::DXGI_FORMAT_D32_FLOAT)
		{
			DepthStencilState.DepthEnable = true;
			DepthStencilState.DepthWriteMask = static_cast<::D3D12_DEPTH_WRITE_MASK>(enableWrite);
			DepthStencilState.DepthFunc = depthFunc;
			DSVFormat = format;

			return *this;
		}
		auto&& StencilState(UINT8 stencilReadMask, UINT8 stencilWriteMask, 
			D3D12_DEPTH_STENCILOP_DESC frontFace = {}, D3D12_DEPTH_STENCILOP_DESC backFace = {})
		{
			DepthStencilState.StencilReadMask= stencilReadMask;
			DepthStencilState.StencilWriteMask = stencilWriteMask;
			DepthStencilState.FrontFace = frontFace;
			DepthStencilState.BackFace = backFace;

			return *this;
		}
		auto&& InputLayout(::std::vector<::D3D12_INPUT_ELEMENT_DESC> const& elements) 
		{
			MODEL_ASSERT(EnableInputAssembly, "Root signature do not allow input layout.");
			InputElements = elements;
			base_t::InputLayout = { InputElements.data(), static_cast<::UINT>(InputElements.size()) };

			return *this;
		}
		// abandoned in future.
		auto&& StreamOutput(::std::vector<::D3D12_SO_DECLARATION_ENTRY> const& entry, ::std::vector<::UINT> strides, ::UINT streamIndex)
		{
			MODEL_ASSERT(EnableStreamOutput, "Root signature do not allow stream output.");
			Strides = strides;
			StreamOutputEntry = entry;
			base_t::StreamOutput = 
			{ 
			  StreamOutputEntry.data()
			, static_cast<::UINT>(StreamOutputEntry.size()), Strides.data()
			, static_cast<::UINT>(Strides.size()), 
			streamIndex 
			};
			
			return *this;
		}
		auto&& Else(::D3D12_PRIMITIVE_TOPOLOGY_TYPE topology, 
			D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ibStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED)
		{
			base_t::PrimitiveTopologyType = topology;
			base_t::IBStripCutValue = ibStripCutValue;

			return *this;
		}
		auto&& SetNodeMask(::UINT nodeMask) 
		{
			base_t::NodeMask = nodeMask;

			return *this;
		}

		operator::D3D12_GRAPHICS_PIPELINE_STATE_DESC const* () const { return this; }
	private:
		bool EnableStreamOutput;
		bool EnableInputAssembly;

		::std::vector<::D3D12_INPUT_ELEMENT_DESC> InputElements;
		::std::vector<::D3D12_SO_DECLARATION_ENTRY> StreamOutputEntry;
		::std::vector<::UINT> Strides;
	};

	struct ComputePipelineBuilder : ::D3D12_COMPUTE_PIPELINE_STATE_DESC
	{
		ComputePipelineBuilder() = default;
	};
}

TWEN_EXPORT namespace twen
{
	class Pipeline
		: public Residency::Resident
		, public inner::MultiNodeObject
		, public inner::ShareObject<Pipeline>
		, public inner::DeviceChild
	{
	protected:
		Pipeline(Device& device, ::UINT visibleNode, ID3D12PipelineState* state)
			: inner::DeviceChild{ device }
			, inner::MultiNodeObject{ visibleNode, device.NativeMask }
			, Resident{ false, resident::PipelineState } // Move this '0u' after finish pipeline generator.
			, m_Handle{ state }
		{ Resident::Size = Cache()->GetBufferSize(); }

	public:

		Pipeline(Pipeline const&) = delete;
		Pipeline& operator=(Pipeline const&) = delete;

		~Pipeline() { m_Handle->Release(); }

		ComPtr<::ID3DBlob> Cache() const
		{
			ComPtr<::ID3DBlob> result;
			m_Handle->GetCachedBlob(result.Put());
			MODEL_ASSERT(result, "Get pipeline state's cache failure.");
			return result;
		}

		operator::ID3D12PipelineState* () const
		{
			GetDevice().UpdateResidency(this);
			return m_Handle;
		}
	protected:
		::ID3D12PipelineState* m_Handle;
	};
	// TODO: Seperate graphics pipeline desc into some pieces.
	class GraphicsPipelineState
		: public Pipeline
	{
	public:
		GraphicsPipelineState(Device& device, inner::GraphicsPipelineBuilder const& builder, ::ID3D12PipelineState* state) 
			: Pipeline{device, builder.NodeMask, state}
			, RenderTargetCount{builder.NumRenderTargets}
			, DepthEnable{static_cast<bool>(builder.DepthStencilState.DepthEnable)}
			, StencilEnable{static_cast<bool>(builder.DepthStencilState.StencilEnable)}
		{}
	public:
		const::UINT RenderTargetCount : 4;
		const bool DepthEnable : 1;
		const bool StencilEnable : 1;
	};

	class DECLSPEC_EMPTY_BASES ComputePipelineState
		: public Pipeline
	{
	public:
		ComputePipelineState(Device& device, ::UINT nodeMask, ::ID3D12PipelineState* state)
			: Pipeline{device, nodeMask, state} 
		{}
	};
}

TWEN_EXPORT namespace twen
{
	class PipelineManager
	{
	public:
		// Start to build graphics pipeline state. 
		// In debug: this method will check whether the root signature is empty.
		[[nodiscard("No need to call Build on pipeline manager.")]]
		inner::GraphicsPipelineBuilder Build(RootSignature* rso) const
		{
			MODEL_ASSERT(rso, "Root signature cannot be empty.");

			return {rso}; 
		}
		// Create as graphics pipeline state.
		[[nodiscard("No need to call Create on pipeline manager.")]]
		GraphicsPipelineState* Create(Device& device, inner::GraphicsPipelineBuilder const& builder) 
		{
			::ID3D12PipelineState* state{nullptr};
			device.Verify(
				device->CreateGraphicsPipelineState(builder, IID_PPV_ARGS(&state))
				);

			return device.Create<GraphicsPipelineState>(builder, state);
		}
		// Create as compute pipeline state.
		[[nodiscard("No need to call Create on pipeline manager.")]]
		ComputePipelineState* Create(Device& device, ShaderNode* computeShader, RootSignature* rso, ::UINT nodeMask)
		{
			MODEL_ASSERT(rso, "Rso cannot be empty, Currently.");

			D3D12_COMPUTE_PIPELINE_STATE_DESC desc
			{ *rso, computeShader->Bytecode(), nodeMask, };

			::ID3D12PipelineState* state{ nullptr };
			device.Verify(
				device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&state))
			);
			
			return device.Create<ComputePipelineState>(nodeMask, state);
		}
	private:
	};
}
