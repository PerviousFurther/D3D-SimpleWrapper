#pragma once

namespace twen::Roots
{
	enum ParameterType
	{
		// root descriptor type.
		RConst = ::D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
		RSrv = ::D3D12_ROOT_PARAMETER_TYPE_SRV,
		RUav = ::D3D12_ROOT_PARAMETER_TYPE_UAV,
		RCbv = ::D3D12_ROOT_PARAMETER_TYPE_CBV,
		// Table descriptor type.
		TSrv = (::D3D12_ROOT_PARAMETER_TYPE_UAV + 1) + ::D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		TUav = (::D3D12_ROOT_PARAMETER_TYPE_UAV + 1) + ::D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
		TCbv = (::D3D12_ROOT_PARAMETER_TYPE_UAV + 1) + ::D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		TSampler = (::D3D12_ROOT_PARAMETER_TYPE_UAV + 1) + ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,

	};

	inline constexpr auto ParameterTypeFrom(::D3D12_ROOT_PARAMETER_TYPE type) 
	{
		return static_cast<ParameterType>(type);
	}
	inline constexpr auto ParameterTypeFrom(::D3D12_DESCRIPTOR_RANGE_TYPE type)
	{
		return static_cast<ParameterType>(type + 1 + RUav);
	}

	inline::UINT16 StaticSamplerCode(::UINT reg, ::UINT space) { return static_cast<::UINT8>(((reg & 0xff)) | ((space & 0xff) << 8u)); }

	// @return pair: [register, space]
	inline::std::pair<::UINT8, ::UINT8> StaticSamplerCode(::UINT encode)
	{
		return { static_cast<::UINT8>(encode & 0xff), static_cast<::UINT8>((encode >> 8u) & 0xff) };
	}

	// encode root signature key.
	// by following structure:
	// type : 16 | bindingPoint : 8 | registerSpace : 8
	// H -------------------------------------------- L
	inline constexpr::UINT32 RootSignatureKey(::UINT bindingPoint, ::UINT registerSpace, ParameterType type)
	{
		return ((type & 0xff) << 16) | (bindingPoint & 0xff) << 8 | (registerSpace & 0xff);
	}
	// decode root signature key.
	// see other overload for structure.
	inline constexpr auto RootSignatureKey(::UINT32 value)
	{
		struct
		{
			::UINT8 BindingPoint;
			::UINT8 RegisterSpace;
			ParameterType Type;
		} result
		{
			static_cast<::UINT8>((value >> 8) & 0xff),
			static_cast<::UINT8>(value & 0xff),
			static_cast<ParameterType>((value >> 16) & 0xff)
		};

		return result;
	}

	// Obtain profiled root descriptor's flags.
	inline constexpr auto Flags(::D3D12_ROOT_PARAMETER_TYPE type)
	{
		switch (type)
		{
		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
			return::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		case D3D12_ROOT_PARAMETER_TYPE_CBV:
			return::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		case D3D12_ROOT_PARAMETER_TYPE_SRV:
			return::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		case D3D12_ROOT_PARAMETER_TYPE_UAV:
			return::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;

		default:return::D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		}
	}
	// Obtain profiled descriptor range's flags.
	inline static constexpr auto Flags(::D3D12_DESCRIPTOR_RANGE_TYPE type)
	{
		switch (type)
		{
		case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
			return::D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
			return::D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
			return::D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		default:
			return::D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
		}
	}

	struct Descriptor 
	{
		::UINT8 RootIndex  { 0xff };
		::UINT8 TableIndex { 0xff };

		bool OnTable() const { return TableIndex != 0xff; }
		bool OnRoot()  const { return TableIndex == 0xff; }

		::UINT8 Count      { 0u };
		::UINT8 Padding;
	};

#if D3D12_MODEL_DEBUG
	struct RootSnapshot
	{
		::D3D12_SHADER_VISIBILITY Visiblity;
		::D3D12_ROOT_PARAMETER_TYPE Type;
		::D3D12_ROOT_DESCRIPTOR_FLAGS Flags;
	};

	struct TableSnapshot
	{
		::D3D12_SHADER_VISIBILITY Visibility;
		::D3D12_DESCRIPTOR_RANGE_TYPE Type;
		::D3D12_DESCRIPTOR_RANGE_FLAGS Flags;

		struct less
		{
			bool operator()(::UINT16 const& left, ::UINT16 const& right)
			{
				auto leftH{ left & 0xff00 }, rightH{ right & 0xff00 };
				return leftH == rightH ? ((right & 0xff) < (left & 0xff)) : leftH < rightH;
			}
		};
	};
#endif
}

namespace twen
{
	class RootSignature;

	template<typename...Args>
	::std::shared_ptr<RootSignature> CreateRootSignature(Device& device, ::D3D12_ROOT_SIGNATURE_FLAGS flags, Args const&...args);

	// TODO: Discard at finished static root signature.
	class RootSignatureBuilder
	{
		friend class RootSignature;
		friend class Pipeline;
	public:
		RootSignatureBuilder() = default;

		inline auto& BeginTable()
		{
			assert(Size < 64u && "Cannot add any descriptor.");
			MODEL_ASSERT(NumDescriptors == -1, "Must end descriptor table at first.");

			Size += 1u;
			NumDescriptors = 0;

			// Append signature parameter.
			Ranges.emplace_back();
			Offset = static_cast<::UINT>(Params.size());
			Params.emplace_back(); 

			return *this;
		}

		// Each descriptor range must be ascending order.
		inline auto& AddRange(::D3D12_DESCRIPTOR_RANGE_TYPE type, ::UINT nums, ::UINT base, ::UINT space)
		{
		#define SAMPLER_ ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER

			MODEL_ASSERT(nums < 256u, "Too many descriptors.");
			MODEL_ASSERT(NumDescriptors != -1, "Begin descriptor wasn't be invoked.");
			MODEL_ASSERT(!NumDescriptors || (Ranges.back().back().RangeType == SAMPLER_ ? type == SAMPLER_ : type != SAMPLER_),
				"Descriptor range must be in same type if last range type is sampler, otherwise, type of new range cannot be sampler.");

			auto rootIndex  = Offset;
			auto tableIndex = static_cast<::UINT8>(Ranges.back().size());

			// add descriptor. (6.5)
			for(auto i{0u}; i < nums; ++i)
				m_DescriptorInfo.emplace(
					Roots::RootSignatureKey(base + i, space, Roots::ParameterTypeFrom(type)),
					static_cast<::UINT>(m_Descriptors.size()));
			m_Descriptors.emplace_back(static_cast<::UINT8>(rootIndex), static_cast<::UINT8>(tableIndex), static_cast<::UINT8>(nums));

			// Create signature parameter.
			Ranges.back().emplace_back(type, nums, base, space, Roots::Flags(type), NumDescriptors);

			NumDescriptors += nums;
			return *this;

		#undef SAMPLER_
		}

		inline auto& EndTable(::D3D12_SHADER_VISIBILITY visbility = ::D3D12_SHADER_VISIBILITY_ALL)
		{
			MODEL_ASSERT(NumDescriptors != -1, "Begin descriptor wasn't be invoked.");
			MODEL_ASSERT(NumDescriptors, "Empty range.");
			
			const bool isSampler{Ranges.back().back().RangeType == ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER};

			// Add craete signature parameter.
			Params.at(Offset) = 
				::D3D12_ROOT_PARAMETER1
				{
					.ParameterType{::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE},
					.DescriptorTable{static_cast<::UINT>(Ranges.back().size()), Ranges.back().data()},
					.ShaderVisibility{visbility},
				};

			// add count.
			(void)(isSampler ? NumDescriptorOnRangeSampler += NumDescriptors : NumDescriptorOnRangeCSU += NumDescriptors);

			m_RootAllocationInfo.emplace_back(static_cast<::UINT16>(NumDescriptors));

			NumDescriptors = -1;
			Offset = 0u;

			return *this;
		}

		// Each root's register must be ascending order.
		inline auto& AddRoot(::D3D12_ROOT_PARAMETER_TYPE type, ::UINT reg, ::UINT space,
			::D3D12_SHADER_VISIBILITY visibility = ::D3D12_SHADER_VISIBILITY_ALL)
		{
			MODEL_ASSERT(Size < 63u, "Cannot add root descriptor.");
			Size += 2u;

			D3D12_ROOT_PARAMETER1 param
			{
			.ParameterType{type},
			.Descriptor
			{ static_cast<::UINT8>(reg & 0xff), static_cast<::UINT8>(space & 0xff), Roots::Flags(type)},
			.ShaderVisibility{visibility}
			};
			::UINT8 root{ static_cast<::UINT8>(Params.size()) };

			// add descriptor.
			m_DescriptorInfo.emplace(Roots::RootSignatureKey(reg, space, Roots::ParameterTypeFrom(type)), static_cast<::UINT>(m_Descriptors.size()));
			m_Descriptors.emplace_back(static_cast<::UINT8>(root), static_cast<::UINT8>(0xffu), static_cast<::UINT8>(1u));

			// Add create signature parameter.
			Params.emplace_back(param);

			m_RootAllocationInfo.emplace_back(static_cast<::UINT16>(0u));

			return *this;
		}

		inline auto& AddStaticSampler(::D3D12_STATIC_SAMPLER_DESC const& desc)
		{
			// Create signature parameter.
			auto const& sampler{ StaticSamplers.emplace_back(desc) };

			// Helper.
			m_StaticSamplers.emplace(Roots::StaticSamplerCode(desc.ShaderRegister, desc.RegisterSpace), sampler);

			return *this;
		}
		inline auto& AddStaticSampler(::D3D12_SAMPLER_DESC const& desc, ::UINT reg, ::UINT space, 
			::D3D12_STATIC_BORDER_COLOR borderColor =::D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
			::D3D12_SHADER_VISIBILITY visiblity = ::D3D12_SHADER_VISIBILITY_ALL)
		{
			auto const& sampler{ StaticSamplers.emplace_back(Transform(desc, reg, space, visiblity, borderColor)) };

			m_StaticSamplers.emplace(Roots::StaticSamplerCode(reg, space), sampler);

			return *this;
		}

		inline auto GetDesc(::D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) const
		{
			MODEL_ASSERT(NumDescriptors == -1, "Should call EndTable([shader visiblity]) before calling GetDesc.");
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

		template<typename...Args>
		friend::std::shared_ptr<RootSignature> 
			CreateRootSignature(Device& device, ::D3D12_ROOT_SIGNATURE_FLAGS flags, Args const&...args);
	private:
		// Only for descriptor table. specified the root index.
		::UINT Offset{ 0u };

		// Only for descriptor table. specified table bound descriptor nums.
		::INT NumDescriptors{ -1 };

		// Total size.
		::UINT Size{ 0u };

		::UINT NumDescriptorOnRangeCSU{0u};
		::UINT NumDescriptorOnRangeSampler{0u};

		::std::vector<::D3D12_ROOT_PARAMETER1> Params;                  // Place for create the root signature.
		::std::vector<::std::vector<::D3D12_DESCRIPTOR_RANGE1>> Ranges;	// Place for create the root signature.
		::std::vector<::D3D12_STATIC_SAMPLER_DESC> StaticSamplers;      // Place for create the root signature.

		// info of static sampler.
		::std::unordered_map<::UINT16, 
			::D3D12_STATIC_SAMPLER_DESC> m_StaticSamplers;
		
		// data of desctiptor.
		::std::vector<Roots::Descriptor> m_Descriptors;
		// Map by register space to descriptor.
		::std::unordered_map<::UINT32, ::UINT> m_DescriptorInfo;

		::std::vector<::UINT16> m_RootAllocationInfo;
	#if D3D12_MODEL_DEBUG
		::std::unordered_map<::UINT8, Roots::RootSnapshot> m_RootSnapshots;
	#endif
	};

	class RootSignature : public inner::ShareObject<RootSignature>
	{
	public:
		RootSignature(::ID3D12RootSignature* rso, ::ID3DBlob* blob, 
			RootSignatureBuilder const& builder, ::D3D12_ROOT_SIGNATURE_FLAGS flags);

		RootSignature(RootSignature const&) = delete;
		RootSignature& operator=(RootSignature const&) = delete;

		~RootSignature() 
		{
			m_Handle->Release();
			m_Blob->Release();
		}
	public:
		auto const& Map(::UINT reg, ::UINT space, Roots::ParameterType type) const 
		{ 
			MODEL_ASSERT(m_DescriptorInfo.contains(Roots::RootSignatureKey(reg, space, type)), "Not recorded by this root signature.");
			return m_Descriptors.at(
				m_DescriptorInfo.at(Roots::RootSignatureKey(reg, space, type)
				)); 
		}

		auto Assert(::UINT root, Roots::ParameterType type, Shaders::Type) 
		{
		#if D3D12_MODEL_DEBUG
			MODEL_ASSERT(m_RootSnapshots.contains(static_cast<::UINT8>(root & 0xff)), "Not vaild root.");
			MODEL_ASSERT(Roots::ParameterTypeFrom(m_RootSnapshots.at(static_cast<::UINT8>(root & 0xff)).Type) == type, "Mismatch parameter type.");
			//TOOD: Check shader visible.
		#endif
		}

		auto Assert(::UINT, ::UINT, Roots::ParameterType, Shaders::Type) 
		{
		#if D3D12_MODEL_DEBUG
			//TOOD: do Check.
		#endif
		}

		::UINT16 NumAllocation(::UINT index) const 
		{ 
			MODEL_ASSERT(index < m_RootsAllocationInfo.size(), "Out of range.");
			return m_RootsAllocationInfo.at(index); 
		}

		bool SamplerIsStatic(::UINT reg, ::UINT space) 
		{ return m_StaticSamplers.contains(Roots::StaticSamplerCode(reg, space)); }

		operator::ID3D12RootSignature*() const { return m_Handle; }

	public:

		const::D3D12_ROOT_SIGNATURE_FLAGS Flags;

	private:

		// Helper data for searching.

		// data of desctiptor.
		::std::vector<Roots::Descriptor> m_Descriptors;
		// Map by register space to descriptor.
		::std::unordered_map<::UINT32, ::UINT> m_DescriptorInfo;
		// Static samplers.
		::std::unordered_map<::UINT16, ::D3D12_STATIC_SAMPLER_DESC> m_StaticSamplers;
		// Describe num of descriptor should descriptor heap allocate in root.
		::std::vector<::UINT16> m_RootsAllocationInfo;
	#if D3D12_MODEL_DEBUG
		::std::unordered_map<::UINT8, Roots::RootSnapshot> m_RootSnapshots;
	#endif
		// D3D12 handles.

		::ID3D12RootSignature* m_Handle{ nullptr };
		::ID3DBlob* m_Blob{ nullptr };
	};
	inline RootSignature::RootSignature(::ID3D12RootSignature* rootSignature, ::ID3DBlob* blob,
		RootSignatureBuilder const& builder, ::D3D12_ROOT_SIGNATURE_FLAGS flags)
		: m_Handle{rootSignature}
		, m_Blob{ blob }
		, Flags{ flags }
		, m_StaticSamplers{builder.m_StaticSamplers}
		, m_DescriptorInfo{builder.m_DescriptorInfo}
		, m_Descriptors{builder.m_Descriptors}
		, m_RootsAllocationInfo{builder.m_RootAllocationInfo}
	{}

	inline::std::shared_ptr<RootSignature> RootSignatureBuilder::Create(Device& device, ::D3D12_ROOT_SIGNATURE_FLAGS flags =::D3D12_ROOT_SIGNATURE_FLAG_NONE)
	{
		auto desc = GetDesc(flags);

		::ID3D12RootSignature* rso{nullptr};
		::ID3DBlob* blob{nullptr}, *error{ nullptr};
		if (SUCCEEDED(::D3D12SerializeVersionedRootSignature(&desc, &blob, &error)))
		{
			device->CreateRootSignature(device.NativeMask, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rso));
			if (!rso) 
			{
				if (blob) 
					blob->Release();
				if (error) 
					error->Release();
				MODEL_ASSERT(false, "Create root signature failure.");
			}
		} else {
			::std::string message{"Unknown error."};
			if (error) 
			{
				message = static_cast<const char*>(error->GetBufferPointer());
				error->Release();
			}
			if (blob) blob->Release();
		#if D3D12_MODEL_DEBUG
			MODEL_ASSERT(false, "Serialize root signature failure.");
		#else
			throw::std::runtime_error(message);
		#endif
		}

		return::std::make_shared<RootSignature>(rso, blob, *this, flags);
	}

}
