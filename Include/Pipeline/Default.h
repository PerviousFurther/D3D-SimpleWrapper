#pragma once

namespace twen
{
	inline namespace SamplerState
	{
		// Sampler state which sample an texture with a black border, no comparsion and ignoring mipmap.
		inline constexpr::D3D12_SAMPLER_DESC BlackBorder
		{
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			0.0f, 1u, D3D12_COMPARISON_FUNC_NEVER,
			{ }, 0.0f, D3D12_FLOAT32_MAX,
		};
		// Sampler state which sample an texture with no comparsion, ignoring mipmap and wrap address mode.
		inline constexpr::D3D12_SAMPLER_DESC NormalWrap
		{
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			0.0f, 1u, D3D12_COMPARISON_FUNC_NEVER,
			{ }, 0.0f, D3D12_FLOAT32_MAX,
		};

		inline constexpr struct
		{
			inline constexpr::D3D12_STATIC_SAMPLER_DESC operator()(::D3D12_SAMPLER_DESC const& desc,
				::UINT reg, ::UINT regSpace, ::D3D12_SHADER_VISIBILITY visibility, 
				::D3D12_STATIC_BORDER_COLOR borderColor =::D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK) const
			{ 
				return
				{
					desc.Filter, desc.AddressU, desc.AddressV, desc.AddressW,
					desc.MipLODBias, desc.MaxAnisotropy, desc.ComparisonFunc,
					borderColor, desc.MinLOD, desc.MaxLOD, reg, regSpace, visibility
				};
			}
			inline constexpr::D3D12_SAMPLER_DESC operator()(::D3D12_STATIC_SAMPLER_DESC const& desc) 
			{
				::D3D12_SAMPLER_DESC result
				{
					desc.Filter,
					desc.AddressU,
					desc.AddressV,
					desc.AddressW,
					desc.MipLODBias,
					desc.MaxAnisotropy,
					desc.ComparisonFunc,
					{},
					desc.MinLOD,
					desc.MaxLOD
				};

				switch (desc.BorderColor)
				{
				case::D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK:
					result.BorderColor[3] = 1.0f;
					break;
				case::D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE:
					::std::ranges::copy(::std::initializer_list{ 1.0f, 1.0f, 1.0f, 1.0f }, result.BorderColor);
					break;
				default:
					break;
				}

				return result;
			}
		} Transform;
	}

	inline namespace RasterizerState
	{
		inline constexpr::D3D12_RASTERIZER_DESC NoCull_Soild
		{
			::D3D12_FILL_MODE_SOLID, ::D3D12_CULL_MODE_NONE,
		};
		inline constexpr::D3D12_RASTERIZER_DESC CounterClockFrontSolid
		{
			::D3D12_FILL_MODE_SOLID, ::D3D12_CULL_MODE_BACK, true
		};
	}

	inline namespace BlendState 
	{
		inline constexpr::D3D12_RENDER_TARGET_BLEND_DESC RT_NoBlendRGBA{ 
			.BlendEnable			{false}, 
			.LogicOpEnable			{false}, 
			.RenderTargetWriteMask	{static_cast<::UINT8>(D3D12_COLOR_WRITE_ENABLE_ALL)}
		};
		inline constexpr::D3D12_BLEND_DESC NoBlend_RGBA{ false, false, {RT_NoBlendRGBA} };
		//inline constexpr::D3D12_BLEND_DESC NoBlendMultiRGBA{ false, false, {RT_NoBlendRGBA,} };
	}
}