#pragma once

#include "Bindable.hpp"

namespace twen::Samplers 
{
	struct DECLSPEC_EMPTY_BASES SamplerView : ShareObject<SamplerView>, DeviceChild
	{
	public:
		using view_t = ::D3D12_SAMPLER_DESC;

		static constexpr auto CreateView{ &::ID3D12Device::CreateSampler };

		static constexpr auto DescriptorHeapType{ ::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER };
		static constexpr auto RangeType{ ::D3D12_DESCRIPTOR_RANGE_TYPE_SRV };

		static consteval::D3D_SHADER_INPUT_TYPE ShaderInputType() noexcept { return::D3D_SHADER_INPUT_TYPE::D3D_SIT_SAMPLER; }
	protected:
		SamplerView(Device& device) : DeviceChild{device} {}
	};
}

TWEN_EXPORT namespace twen
{
	// Dynamic sampler. 
	class DECLSPEC_EMPTY_BASES Sampler : public Bindings::Bindable<Samplers::SamplerView>
	{
	public:
		Sampler(Device& device, ::D3D12_SAMPLER_DESC const& desc) 
			: Bindable{device} { m_View = desc; }
		Sampler(Device& device) : Bindable{device} {}

		::D3D12_SAMPLER_DESC& View() { return m_View; }
	};
}