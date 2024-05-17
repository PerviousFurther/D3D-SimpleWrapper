#pragma once

#include "Shader.hpp"
//#include "Misc\Hash.hpp"

namespace twen
{
	//template<typename T> 
	// class ShaderReflection;

	class RootSignature;
	struct DirectContext;
	struct ComputeContext;
	struct VertexShader;
	namespace Shaders 
	{
		struct Input; 
	}
	namespace Descriptors 
	{
		inline constexpr struct 
		{
			inline constexpr auto operator()(::D3D12_ROOT_PARAMETER_TYPE type) const
			{
				switch (type)
				{
				case D3D12_ROOT_PARAMETER_TYPE_CBV:
					return::D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				case D3D12_ROOT_PARAMETER_TYPE_SRV:
					return::D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				case D3D12_ROOT_PARAMETER_TYPE_UAV:
					return::D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				default:
					return::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				}
			}
		} Transform{};

		struct Range
		{
			::D3D12_DESCRIPTOR_RANGE_TYPE Type : 3;
			::UINT Count : 5;
			::UINT Register : 4;
			::UINT Space : 4;

			void Attach(Shaders::Input& input)&;
			void Detach();

			mutable Shaders::Input* BindedInput{nullptr};

			operator unsigned short() const { return static_cast<::UINT16>((Type << 13) | (Count << 8) | (Register << 4) | Space); }
			operator Shaders::Input() const;
			operator::D3D12_DESCRIPTOR_RANGE_TYPE() const { return Type; }

			friend int operator<=>(Range const& range, Shaders::Input const& input);
		};

		struct Root
		{
			Root(::D3D12_ROOT_PARAMETER1 const& param) 
				: Type{param.ParameterType}
				, Parameter{
					static_cast<::D3D12_DESCRIPTOR_RANGE_TYPE>(Utils.RootTypeToRangeType(param.ParameterType)), 
					1u,
					param.Descriptor.ShaderRegister, 
					param.Descriptor.RegisterSpace, 
				}
			{}

			Root() :Type{ ::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE }, Ranges{} {}
			Root(Root const& other) 
			{
				Type = other.Type;
				switch (other.Type) 
				{
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
					Ranges = other.Ranges;
					break;
				case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				case D3D12_ROOT_PARAMETER_TYPE_CBV:
				case D3D12_ROOT_PARAMETER_TYPE_SRV:
				case D3D12_ROOT_PARAMETER_TYPE_UAV:
					Parameter = other.Parameter;
					break;
				}
			}

			~Root() 
			{
				switch (Type)
				{
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
					Ranges.~vector();
					break;
				case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				case D3D12_ROOT_PARAMETER_TYPE_CBV:
				case D3D12_ROOT_PARAMETER_TYPE_SRV:
				case D3D12_ROOT_PARAMETER_TYPE_UAV:
				default:
					Parameter.~Range();
					break;
				}
			}

			::D3D12_ROOT_PARAMETER_TYPE Type : 4{::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE};
			union 
			{
				Range Parameter;
				::std::vector<Range> Ranges{};
			};
		};

		struct Static
		{
			::UINT Register;
			::UINT Space;

			Shaders::Input* BindedInput;
		};
	}

	class RootSignatureBuilder
	{
		friend class RootSignature;
		friend class GraphicsPipelineState;
		friend class ComputePipelineState;
	public:
		inline auto& BeginTable()
		{
			assert(Size < 64u && "Cannot add any descriptor.");
			MODEL_ASSERT(NumDescriptors == -1, "Must end descriptor table at first.");

			Size += 1u;
			NumDescriptors = 0;

			Ranges.emplace_back();
			Roots.emplace_back();

			Offset = Params.size();
			Params.emplace_back();

			return *this;
		}
		inline static constexpr auto Flags(::D3D12_DESCRIPTOR_RANGE_TYPE type) 
		{
			switch (type)
			{
			case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
				return::D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

			case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
				return::D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

			case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
			case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
			default:
				return::D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			}
		}
		inline auto& AddRange(::D3D12_DESCRIPTOR_RANGE_TYPE type, ::UINT nums, ::UINT base, ::UINT space)
		{
		#define SAMPLER ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER

			MODEL_ASSERT(NumDescriptors != -1, "Begin descriptor wasn't be invoked.");
			MODEL_ASSERT(!NumDescriptors || (Ranges.back().back().RangeType == SAMPLER ? type == SAMPLER : type != SAMPLER),
				"Descriptor range must be in same type if last range type is sampler, otherwise, type of new range cannot be sampler.");

			Ranges.back().emplace_back(type, nums, base, space, Flags(type), NumDescriptors);

			auto& range{ Roots.back().Ranges };
			range.emplace_back(type, nums, base, space);

			NumDescriptors += nums;

			return *this;

		#undef SAMPLER
		}

		inline auto& EndTable(::D3D12_SHADER_VISIBILITY visbility = ::D3D12_SHADER_VISIBILITY_ALL)
		{
			assert(NumDescriptors != -1 && "Begin descriptor wasn't be invoked.");
			assert(NumDescriptors && "Empty range.");

			const bool isSampler{Ranges.back().back().RangeType == ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER};

			Params.at(Offset) = 
				::D3D12_ROOT_PARAMETER1
				{
					.ParameterType{::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE},
					.DescriptorTable{static_cast<::UINT>(Ranges.back().size()), Ranges.back().data()},
					.ShaderVisibility{visbility},
				};

			(void)(isSampler ? NumDescriptorOnRangeSampler += NumDescriptors : NumDescriptorOnRangeCSU += NumDescriptors);

			NumDescriptors = -1;
			Offset = 0u;

			return *this;
		}
		// For simplified add root parameter operation.
		inline static constexpr auto Flags(::D3D12_ROOT_PARAMETER_TYPE type)
		{
			switch (type)
			{
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				return::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

			case D3D12_ROOT_PARAMETER_TYPE_CBV:
				return::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

			case D3D12_ROOT_PARAMETER_TYPE_SRV:
				return::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

			case D3D12_ROOT_PARAMETER_TYPE_UAV:
				return::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;

			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			default:return::D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
			}
		}
		inline auto& AddRoot(::D3D12_ROOT_PARAMETER_TYPE type, ::UINT reg, ::UINT space,
			::D3D12_SHADER_VISIBILITY visibility = ::D3D12_SHADER_VISIBILITY_ALL)
		{
			assert(Size < 63u && "Cannot add root descriptor.");
			Size += 2u;

			D3D12_ROOT_PARAMETER1 param
			{
				.ParameterType{type},
				.Descriptor
				{
					reg, space, Flags(type)
				},
				.ShaderVisibility{visibility}
			};

			Params.emplace_back(param);
			Roots.emplace_back(param);

			return *this;
		}

		inline auto& AddStaticSampler(::D3D12_STATIC_SAMPLER_DESC const& desc)
		{
			StaticSamplers.emplace_back(desc);
			return *this;
		}
		inline auto& AddStaticSampler(::D3D12_SAMPLER_DESC const& desc, ::UINT reg, ::UINT space, 
			::D3D12_STATIC_BORDER_COLOR borderColor =::D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
			::D3D12_SHADER_VISIBILITY visiblity = ::D3D12_SHADER_VISIBILITY_ALL)
		{
			StaticSamplers.emplace_back(SamplerState::ToStatic(desc, reg, space, visiblity, borderColor));
			return *this;
		}

		inline auto GetDesc(::D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) const
		{
			assert(NumDescriptors == -1 && "Should call EndTable([shader visiblity]) before calling GetDesc.");
			return::D3D12_VERSIONED_ROOT_SIGNATURE_DESC
			{
				.Version{::D3D_ROOT_SIGNATURE_VERSION_1_1},
				.Desc_1_1
				{
					static_cast<::UINT>(Params.size()), Params.data(),
					static_cast<::UINT>(StaticSamplers.size()), StaticSamplers.data(), flags
				}
			};
		}

		::std::shared_ptr<RootSignature> Create(Device& device, ::D3D12_ROOT_SIGNATURE_FLAGS flags);
	private:
		::std::size_t Offset{ 0u };
		::INT NumDescriptors{ -1 };
		::UINT Size{ 0u };

		::UINT NumDescriptorOnRangeCSU{0u};
		::UINT NumDescriptorOnRangeSampler{0u};

		mutable::std::vector<::D3D12_ROOT_PARAMETER1> Params;
		::std::vector<::std::vector<::D3D12_DESCRIPTOR_RANGE1>> Ranges;
		::std::vector<::D3D12_STATIC_SAMPLER_DESC>	StaticSamplers;

		::std::vector<Descriptors::Root> Roots;
	};

	class RootSignature : public ShareObject<RootSignature>
	{
		friend struct DirectContext;
		friend class CommandSignature;
		friend struct ComputeContext;
	public:
		RootSignature(ComPtr<::ID3D12RootSignature> rso, ComPtr<::ID3DBlob> blob, 
			RootSignatureBuilder const& builder, ::D3D12_ROOT_SIGNATURE_FLAGS flags);
	public:
		inline auto const& Binding() const { return m_Roots; }
		inline auto const& Binding(::UINT index) const { assert(m_Roots.size() < index); 
														 return m_Roots.at(index); }

		auto Requirement(::D3D12_DESCRIPTOR_HEAP_TYPE type) const { return m_NumDescriptors.at(type); }

		operator::ID3D12RootSignature*() const { return m_Handle.Get(); }
	public:
		const::D3D12_ROOT_SIGNATURE_FLAGS Flags;
	private:
		::std::vector<Descriptors::Root> m_Roots;
		::std::vector<Descriptors::Static> m_Samplers;

		::std::unordered_map<::D3D12_DESCRIPTOR_HEAP_TYPE, ::UINT> m_NumDescriptors;

		ComPtr<::ID3D12RootSignature> m_Handle{ nullptr };
		ComPtr<::ID3DBlob> m_Blob{ nullptr };
	};
	inline RootSignature::RootSignature(ComPtr<::ID3D12RootSignature> rootSignature, ComPtr<::ID3DBlob> blob,
		RootSignatureBuilder const& builder, ::D3D12_ROOT_SIGNATURE_FLAGS flags)
		: m_Roots{ builder.Roots }
		, m_Handle{rootSignature}
		, m_Blob{ blob }
		, Flags{ flags }
		, m_NumDescriptors{ {::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, builder.NumDescriptorOnRangeCSU}, {::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, builder.NumDescriptorOnRangeSampler} }
	{}

	inline::std::shared_ptr<RootSignature> RootSignatureBuilder::Create(Device& device, ::D3D12_ROOT_SIGNATURE_FLAGS flags)
	{
		auto desc = GetDesc(flags);
		ComPtr<::ID3D12RootSignature> rso;
		ComPtr<::ID3DBlob> blob, error;
		if (SUCCEEDED(::D3D12SerializeVersionedRootSignature(&desc, blob.Put(), error.Put())))
		{
			device->CreateRootSignature(device.NativeMask, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rso.Put()));
			MODEL_ASSERT(rso, "Create root signature failure.");
		} else {
			::std::string message = error ? static_cast<const char*>(error->GetBufferPointer()) : "Unknown error.";
			MODEL_ASSERT(false, "Serialize root signature failure.");
		}
		return RootSignature::Create(rso, blob, *this, flags);
	}

}

//namespace twen
//{
//	//class RootSignatureManager : public::std::enable_shared_from_this<RootSignatureManager>, public DeviceChild
//	//{
//	//public:
//	//	RootSignatureManager(Device& device) : DeviceChild{device} {}
//
//	//	::std::shared_ptr<RootSignature> Create(RootSignatureBuilder& builder, ::D3D12_ROOT_SIGNATURE_FLAGS flags = ::D3D12_ROOT_SIGNATURE_FLAG_NONE);
//	//	::std::shared_ptr<RootSignature> Create(::std::vector<Shaders::Input>const& inputDesc);
//	//private:
//	//	inline auto Generate(::D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc);
//	//private:
//	//	::std::vector<::std::shared_ptr<RootSignature>> m_RecordedSignature;
//	//};
//
//	//inline auto RootSignatureManager::Generate(::D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc)
//	//{
//	//	struct 
//	//	{
//	//		ComPtr<::ID3D12RootSignature> Result;
//	//		ComPtr<::ID3DBlob> Blob;
//	//	} result;
//	//	ComPtr<::ID3DBlob> error;
//	//	auto& device{ GetDevice() };
//
//	//	if (SUCCEEDED(::D3D12SerializeVersionedRootSignature(&desc, result.Blob.Put(), error.Put())))
//	//	{
//	//		device->CreateRootSignature(device.NativeMask, result.Blob->GetBufferPointer(), result.Blob->GetBufferSize(), IID_PPV_ARGS(result.Result.Put()));
//	//		assert(result.Result && "Create root signature failure.");
//	//	} else {
//	//		::std::string message = error ? static_cast<const char*>(error->GetBufferPointer()) : "Unknown error.";
//	//		assert(!"Serialize root signature failure.");
//	//	}
//	//	return result;
//	//}
//
//	//inline::std::shared_ptr<RootSignature> RootSignatureManager::Create(
//	//	RootSignatureBuilder& builder, 
//	//	::D3D12_ROOT_SIGNATURE_FLAGS flags)
//	//{
//	//	auto [result, blob] = Generate(builder.GetDesc(flags));
//	//	return RootSignature::Create(result, blob, builder, flags);
//	//}
//
//	//inline::std::shared_ptr<RootSignature> RootSignatureManager::Create(::std::vector<Shaders::Input> const& inputDesc)
//	//{
//	//	RootSignatureBuilder builder{};
//	//	
//	//	for (auto const& input : inputDesc) 
//	//	{
//	//		auto typeRoot{ Shaders::Utils.ShaderInputTypeToRootParameterType(input.Type) };
//	//		if (typeRoot ==::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) 
//	//		{
//	//			if (builder.NumDescriptors == -1) builder.BeginTable();
//
//	//		} else {
//
//	//		}
//	//	}
//	//}
//	//
//}