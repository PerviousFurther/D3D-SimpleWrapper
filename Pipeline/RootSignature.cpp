#include "mpch.h"

#include "System\Device.h"
#include "Descriptor.h"
#include "RootSignature.h"
#include "Command\Context.h"

namespace twen
{
	static constexpr auto U64BitsCount = sizeof(::UINT64) * CHAR_BIT;

	void RootSignature::Init(::D3D12_ROOT_SIGNATURE_DESC1 const& desc)
	{
		::std::span{ desc.pParameters, desc.NumParameters };
		for (auto const& param : ::std::span{ desc.pParameters, desc.NumParameters }) 
		{
			ShaderFrequency frequency{ ConvertToShaderFrequency(param.ShaderVisibility) };
			switch (param.ParameterType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			{
				::std::size_t numOfDescriptor{ 0u };
				::std::size_t index{ m_RootParamsType.size() };
				m_RangeMap.emplace(index, ::std::unordered_set<Range, RangeHash>{});
				for (auto const& table{ param.DescriptorTable }; auto const& range : ::std::span{ table.pDescriptorRanges, table.NumDescriptorRanges })
				{
					using Descriptor::Range;
					(range.RangeType != ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? m_DescriptorsCount : m_DynamicSamplerCount) += range.NumDescriptors;
					switch (range.RangeType)
					{
					case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
						m_Reg[frequency + ShaderRegister::S] |= ((1 << range.NumDescriptors) - 1) << range.BaseShaderRegister;
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
						m_Reg[NumSlot - 1u] |= ((1 << range.NumDescriptors) - 1) << range.BaseShaderRegister;
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
						m_Reg[frequency + ShaderRegister::B] |= ((1 << range.NumDescriptors) - 1) << range.BaseShaderRegister;
						m_ConstantBufferCount += range.NumDescriptors;
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
						m_Reg[frequency + ShaderRegister::S] |= ((1 << range.NumDescriptors) - 1) << range.BaseShaderRegister;
						break;
					default:assert(!"Unknown range type."); break;
					}
					m_RangeMap.at(index).emplace(range.RangeType, numOfDescriptor, numOfDescriptor + range.NumDescriptors);
					numOfDescriptor += range.NumDescriptors;
				}
			}
			break;
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:break;
			case D3D12_ROOT_PARAMETER_TYPE_CBV:
				m_Reg[frequency + ShaderRegister::RB] |= (1 << param.Descriptor.ShaderRegister);
				m_ConstantBufferCount += 1u;
				break;
			case D3D12_ROOT_PARAMETER_TYPE_SRV:
				m_Reg[frequency + ShaderRegister::S] |= (1 << param.Descriptor.ShaderRegister);
				break;
			case D3D12_ROOT_PARAMETER_TYPE_UAV:
				m_Reg[NumSlot - 1u] |= (1 << param.Descriptor.ShaderRegister);
				break;
			default:assert(!"Unknown root parameter type."); break;
			}
			m_RootParamsType.emplace_back(param.ParameterType);
		}
	}
}

