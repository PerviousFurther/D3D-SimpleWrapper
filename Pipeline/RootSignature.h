#pragma once

namespace twen
{
	//template<typename T> 
	// class ShaderReflection;

	class RootSignature;
	struct DirectContext;
	struct ComputeContext;

	struct RootSignatureBuilder
	{
		auto& BeginTable() {
			Offset = Ranges.size();
			return *this;
		}
		auto& AddRange(::D3D12_DESCRIPTOR_RANGE_TYPE type, ::UINT nums, ::UINT base, ::UINT space)
		{
			Ranges.emplace_back(
				type, nums, base, space,
				type == ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? ::D3D12_DESCRIPTOR_RANGE_FLAG_NONE :
				type == ::D3D12_DESCRIPTOR_RANGE_TYPE_UAV ? D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE :
				::D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
				NumDescriptors
			);
			NumDescriptors += nums;
			return *this;
		}
		auto& EndTable(::D3D12_SHADER_VISIBILITY visbility = ::D3D12_SHADER_VISIBILITY_ALL)
		{
			auto const nums = static_cast<::UINT>(Ranges.size() - Offset);
			Params.push_back({ ::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				D3D12_ROOT_DESCRIPTOR_TABLE1{ nums, ::std::span(Ranges.begin() + Offset, nums).data() },
				visbility });

			Offset = 0u;
			NumDescriptors = 0u;

			return *this;
		}

		auto& AddRoot(::D3D12_ROOT_PARAMETER_TYPE type, ::UINT reg, ::UINT space,
			::D3D12_SHADER_VISIBILITY visibility = ::D3D12_SHADER_VISIBILITY_ALL)
		{
			Params.push_back(::D3D12_ROOT_PARAMETER1{ type,
				{.Descriptor{ reg, space,
					type == ::D3D12_ROOT_PARAMETER_TYPE_CBV ? ::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC
					: ::D3D12_ROOT_DESCRIPTOR_FLAG_NONE }
				},
				visibility });
			return *this;
		}

		auto& AddStaticSampler(::D3D12_STATIC_SAMPLER_DESC const& desc)
		{
			StaticSamplers.emplace_back(desc);
			return *this;
		}

		::D3D12_VERSIONED_ROOT_SIGNATURE_DESC GetDesc(::D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) const
		{
			assert(!Offset || !NumDescriptors && "Call EndTable() first.");
			return { ::D3D_ROOT_SIGNATURE_VERSION_1_1,
				{.Desc_1_1{static_cast<::UINT>(Params.size()), Params.data(),
					static_cast<::UINT>(StaticSamplers.size()), StaticSamplers.data(), flags
				}}
			};
		}
		::std::shared_ptr<RootSignature> Create(Device& device, ::D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) const;

		::std::vector<::D3D12_ROOT_PARAMETER1>	Params;

		::std::size_t Offset{ 0u };
		::UINT NumDescriptors{ 0u };
		::std::vector<::D3D12_DESCRIPTOR_RANGE1>		Ranges;

		::std::vector<::D3D12_STATIC_SAMPLER_DESC>		StaticSamplers;
	};

	inline namespace Descriptor 
	{
		struct Detail;

		struct Range 
		{
			::D3D12_DESCRIPTOR_RANGE_TYPE Type;
			::std::size_t Begin;
			::std::size_t End;

			bool operator==(Range const& other) const
			{ return other.Type == Type && other.Begin == Begin; }
		};
		struct RangeHash 
		{
			inline::std::size_t operator()(Range const& range) const 
			{ return::std::hash<::std::size_t>{}(range.Begin); }
		};
	}

	inline namespace Shader
	{
		enum class ShaderFrequency { VS = 0, PS = 4, DS = 8, HS = 12, GS = 16, MS = 20, AS = 24, ALL = 28 };
		enum class ShaderRegister { T, S, B, RB };
		inline constexpr::UINT operator+(ShaderFrequency fre, ShaderRegister sha)
		{
			return static_cast<::UINT>(fre) + static_cast<::UINT>(sha);
		}

		inline constexpr struct
		{
			inline constexpr auto operator()(::D3D12_SHADER_VISIBILITY visbility) const
			{
				switch (visbility)
				{
				case D3D12_SHADER_VISIBILITY_ALL:		return ShaderFrequency::ALL;
				case D3D12_SHADER_VISIBILITY_VERTEX:	return ShaderFrequency::VS;
				case D3D12_SHADER_VISIBILITY_HULL:		return ShaderFrequency::HS;
				case D3D12_SHADER_VISIBILITY_DOMAIN:	return ShaderFrequency::DS;
				case D3D12_SHADER_VISIBILITY_GEOMETRY:	return ShaderFrequency::GS;
				case D3D12_SHADER_VISIBILITY_PIXEL:		return ShaderFrequency::PS;
				case D3D12_SHADER_VISIBILITY_AMPLIFICATION:
														return ShaderFrequency::AS;
				case D3D12_SHADER_VISIBILITY_MESH:		return ShaderFrequency::MS;
				default:return ShaderFrequency::ALL;
				}
			}
		} ConvertToShaderFrequency;
	}

	class RootSignature : public::std::enable_shared_from_this<RootSignature>, public DeviceChild
	{
	private:
		static constexpr auto NumSlot = ShaderFrequency::ALL + ShaderRegister::RB + 2u;
	public:
		RootSignature(Device& device, ::D3D12_VERSIONED_ROOT_SIGNATURE_DESC const& desc);
	public:
		::UINT SamplerCount()		 const { return m_DynamicSamplerCount; }
		::UINT ConstantBufferCount() const { return m_ConstantBufferCount; }
		::UINT DescriptorsCount()	 const { return m_DescriptorsCount; }

		::std::shared_ptr<RootSignature> CreateSubRootsignature(auto&&...UNFININSHED); 
		
		::D3D12_ROOT_PARAMETER_TYPE RootParamsType(::UINT index) const { return m_RootParamsType.at(index); }
		::std::vector<::D3D12_ROOT_PARAMETER_TYPE> const& RootParamsType() const { return m_RootParamsType; }

		auto const& TableRange(::UINT index) { assert(m_RangeMap.contains(index) && "Current rootsignature position do not have range.");
											   return m_RangeMap.at(index); }

		bool IsSubRootSignature(RootSignature const& UNFININSHED);

		::ID3DBlob* Blob()				 const { return m_Blob.get(); }
		operator::ID3D12RootSignature*() const { return m_Handle.get(); }
	public:
		const bool EnabledInputAssembly : 1;
		const bool EnabledStreamOutput : 1;
		const bool Reserved : 1 {false};
		const bool Reserved1 : 1 {false};
	private:
		void Init(::D3D12_ROOT_SIGNATURE_DESC1 const& desc);
	private:
		::UINT8 m_Reg[NumSlot]{};

		::UINT	m_DescriptorsCount{};
		::UINT	m_ConstantBufferCount{};
		::UINT	m_DynamicSamplerCount{};

		::std::vector<::D3D12_ROOT_PARAMETER_TYPE> m_RootParamsType;
		::std::unordered_map<::std::size_t, 
			::std::unordered_set<Descriptor::Range, Descriptor::RangeHash>> m_RangeMap;

		ComPtr<::ID3D12RootSignature> m_Handle{ nullptr };
		ComPtr<::ID3DBlob> m_Blob{ nullptr };
	};
	// Create by desc.
	inline RootSignature::RootSignature(Device& device, ::D3D12_VERSIONED_ROOT_SIGNATURE_DESC const& desc)
		: DeviceChild{ device }
		, EnabledInputAssembly(desc.Desc_1_1.Flags & ::D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
		, EnabledStreamOutput(desc.Desc_1_1.Flags & ::D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT)
	{
		ComPtr<::ID3DBlob> error;
		if (SUCCEEDED(::D3D12SerializeVersionedRootSignature(&desc, m_Blob.put(), error.put())))
		{
			device->CreateRootSignature(device.NativeMask, m_Blob->GetBufferPointer(), m_Blob->GetBufferSize(), IID_PPV_ARGS(m_Handle.put()));
			assert(m_Handle.get() && "Init root signature failure.");
			Init(desc.Desc_1_1);
		}
		else {
			::std::string message = static_cast<const char*>(error->GetBufferPointer());
			assert(!"Serialize root signature failure.");
		}
	}

	// Helper function.

	inline::std::shared_ptr<RootSignature> RootSignatureBuilder::Create(Device& device, ::D3D12_ROOT_SIGNATURE_FLAGS flags) const
	{
		return::std::make_shared<RootSignature>(device, GetDesc(flags));
	}
}