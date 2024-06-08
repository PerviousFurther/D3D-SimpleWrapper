#pragma once

namespace twen 
{
	struct DECLSPEC_EMPTY_BASES D3D12_DISPATCH_RAYS_ARGUMENTS {};
	struct DECLSPEC_EMPTY_BASES D3D12_CONSTANT_ARGUMENTS {};
	using D3D12_CONSTANT_BUFFER_VIEW_ARGUMENTS  = ::D3D12_GPU_VIRTUAL_ADDRESS;
	using D3D12_UNORDERED_ACCESS_VIEW_ARGUMENTS = ::D3D12_GPU_VIRTUAL_ADDRESS;
	using D3D12_SHADER_RESOURCE_VIEW_ARGUMENTS  = ::D3D12_GPU_VIRTUAL_ADDRESS;
	using D3D12_VERTEX_BUFFER_VIEW_ARGUMENTS    = ::D3D12_VERTEX_BUFFER_VIEW;
	using D3D12_INDEX_BUFFER_VIEW_ARGUMENTS		= ::D3D12_INDEX_BUFFER_VIEW;
}
namespace twen::Descriptors
{
	template<typename T>
	struct Range;
}
namespace twen::Indirects
{
#define DIA_(name) ::D3D12_INDIRECT_ARGUMENT_##name

	struct Argument
	{
		Descriptors::Range<Argument>* BindedRange;

		DIA_(TYPE) Type;
	};

#define MAP_HELPER__(Type) \
template<>\
struct Map<DIA_(TYPE_##Type)> {\
	using type = D3D12_##Type##_ARGUMENTS;\
	static constexpr auto size{sizeof(type) == 1 ? 0u : static_cast<::UINT>(sizeof(type)) };\
	static constexpr auto align{alignof(type) == 1 ? 0u : static_cast<::UINT>(alignof(type))};\
}

	template<DIA_(TYPE) type>
	struct Map
	{
		static_assert(type ? false : false, "Wrong type.");
	};

	template<typename T>
	struct TypeMap
	{
		
	};

	MAP_HELPER__(DRAW);
	MAP_HELPER__(DRAW_INDEXED);
	MAP_HELPER__(DISPATCH);
	MAP_HELPER__(DISPATCH_RAYS);

	MAP_HELPER__(DISPATCH_MESH);
	MAP_HELPER__(SHADER_RESOURCE_VIEW);
	MAP_HELPER__(CONSTANT_BUFFER_VIEW);
	MAP_HELPER__(UNORDERED_ACCESS_VIEW);

	MAP_HELPER__(VERTEX_BUFFER_VIEW);
	MAP_HELPER__(INDEX_BUFFER_VIEW);

	MAP_HELPER__(CONSTANT);

	inline constexpr struct
	{
		::UINT operator()(DIA_(TYPE) type) const
		{
#define HELPER_TWEN(type) \
		case DIA_(TYPE_##type): return Map<DIA_(TYPE_##type)>::size
			switch (type)
			{
				HELPER_TWEN(DRAW);
				HELPER_TWEN(DRAW_INDEXED);
				HELPER_TWEN(SHADER_RESOURCE_VIEW);
				HELPER_TWEN(CONSTANT_BUFFER_VIEW);
				HELPER_TWEN(UNORDERED_ACCESS_VIEW);
				HELPER_TWEN(VERTEX_BUFFER_VIEW);
				HELPER_TWEN(INDEX_BUFFER_VIEW);
				HELPER_TWEN(DISPATCH_MESH);
				HELPER_TWEN(DISPATCH);

			default:return 0u;
			}
#undef HELPER_TWEN
		}
	} SizeOf{};
#undef MAP_HELPER__
}
TWEN_EXPORT namespace twen
{
	struct DirectContext;
	struct ComputeContext;

	template<typename T>
	class CommandSignature;

	struct IndirectArugmentBuilder
	{
#define SECOND_TWEN(x, y) y
#define BOTH_TWEN(x, y) x y

#define DEFINE_TWEN(fnName, type, .../*parameter*/)\
inline auto& fnName(__VA_OPT__(BOTH_TWEN(__VA_ARGS__))){\
	m_Arguments.emplace_back(DIA_(DESC){ .Type{DIA_(TYPE_##type)}, __VA_OPT__(.fnName##View{SECOND_TWEN(__VA_ARGS__)})});\
	this->ResolveBlockSize(Indirects::Map<DIA_(TYPE_##type)>::align);\
	m_SizeOfEachBlock.emplace_back(static_cast<::UINT>(Indirects::Map<DIA_(TYPE_##type)>::size));\
	m_OffsetFromTableStart.emplace_back(m_SizeOfTotalBlock);\
	m_SizeOfTotalBlock += static_cast<::UINT>(Utils::Align2(Indirects::Map<DIA_(TYPE_##type)>::size, m_MaxAlign, true));\
	return *this;\
}
		template<typename T>
		friend class CommandSignature;
	public:
		constexpr IndirectArugmentBuilder() = default;
	#define VertexBufferView VertexBuffer
		DEFINE_TWEN(VertexBuffer, VERTEX_BUFFER_VIEW, ::UINT, slot)
	#undef VertexBufferView
		DEFINE_TWEN(ShaderResource, SHADER_RESOURCE_VIEW, ::UINT, root);
		DEFINE_TWEN(ConstantBuffer, CONSTANT_BUFFER_VIEW, ::UINT, root);
		DEFINE_TWEN(UnorderedAccess, UNORDERED_ACCESS_VIEW, ::UINT, root);
		DEFINE_TWEN(Draw, DRAW);
		DEFINE_TWEN(Dispatch, DISPATCH);

		inline constexpr auto DispatchRays()
		{ m_Arguments.emplace_back(::D3D12_INDIRECT_ARGUMENT_DESC{ .Type{ ::D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS }, }); };

		inline constexpr auto Constant(::UINT root, ::UINT value, ::UINT value32Bit);
		DEFINE_TWEN(DispatchMesh, DISPATCH_MESH);

#undef DEFINE_TWEN
#undef BOTH_TWEN
#undef SECOND_TWEN

	private:
		inline void ResolveBlockSize(::UINT64 newAlignment)
		{
			if (newAlignment > m_MaxAlign)
			{
			#if _HAS_CXX23
				for (auto accumlate{ 0u }; auto&& [size, offset] : 
					::std::views::zip(m_SizeOfEachBlock, m_OffsetFromTableStart))
					accumlate += size = static_cast<::UINT>(Utils::Align2(size, newAlignment, true)),
					offset = accumlate;
			#else
				auto accumlate{ 0u };
				auto itS = m_SizeOfEachBlock.begin();
				auto itO = m_OffsetFromTableStart.begin();
				while (itS != m_SizeOfEachBlock.end()) 
				{
					accumlate += *itS = Align.Align2(*itS, newAlignment, true),
						*itO = accumlate;
					itS++, itO++;
				}
			#endif
				
				m_MaxAlign = newAlignment;
			}
		}
	private:
		::std::vector<DIA_(DESC)>	m_Arguments;
		::std::vector<::UINT>		m_SizeOfEachBlock;
		::std::vector<::UINT>		m_OffsetFromTableStart;

		::std::size_t m_MaxAlign{ 0u };
		::UINT m_SizeOfTotalBlock;
	};

	template<>
	class CommandSignature<void> 
		: public inner::DeviceChild /*, public inner::SingleNodeObject*/
	{
	public:
		static constexpr auto Execute{ &::ID3D12GraphicsCommandList::ExecuteIndirect };
	public:
		using type = void;

		CommandSignature(Device& device, ::std::shared_ptr<RootSignature> rootSignature,
			IndirectArugmentBuilder const& builder, ::UINT nodeMask = 0u)
			: inner::DeviceChild{ device }
			//, m_RootMap{ rootSignature->Roots<Indirects::Argument>() }
			, CommandCount{ static_cast<::UINT>(builder.m_Arguments.size()) }
			, Stride{ builder.m_SizeOfTotalBlock }
			, m_SizeOfEachBlock{ builder.m_SizeOfEachBlock }
			, m_OffsetFromStart{ builder.m_OffsetFromTableStart }
		{
			::D3D12_COMMAND_SIGNATURE_DESC desc{ Stride, CommandCount, builder.m_Arguments.data(), nodeMask };
			device->CreateCommandSignature(&desc, rootSignature ? *rootSignature : nullptr, IID_PPV_ARGS(m_CommandSignature.Put()));

			MODEL_ASSERT(m_CommandSignature, "Create command signature failure.");

			m_RootSignature = rootSignature ? rootSignature : nullptr;
		}
	public:
		// Actually this is a stub that will be forward by context.
		void Bind(DirectContext& context);

		::UINT Offset(::UINT index) const 
		{
			MODEL_ASSERT(index < m_OffsetFromStart.size(), "Out of range.");
			return m_OffsetFromStart.at(index); 
		}

		::UINT Size(::UINT index) const 
		{
			MODEL_ASSERT(index < m_SizeOfEachBlock.size(), "Out of range.");
			return m_SizeOfEachBlock.at(index);
		}
		::std::shared_ptr<RootSignature> EmbeddedRootSignature() const { return m_RootSignature; }
		operator::ID3D12CommandSignature* () const { return m_CommandSignature.Get(); }
	public:
		const::UINT CommandCount;
		const::UINT Stride;
	protected:
		::std::shared_ptr<RootSignature> m_RootSignature;

		//::std::vector<Descriptors::Root<Indirects::Argument>> m_RootMap;

		::std::vector<::UINT> m_SizeOfEachBlock;
		::std::vector<::UINT> m_OffsetFromStart;

		ComPtr<::ID3D12CommandSignature> m_CommandSignature;
	};

//if static reflection support.
#if _HAS_CXX26
	// Reserved for inner usage. 
	// @tparam T: Confined by no static reflection support,
	// T must have an type alias name "type" and is an instantiation of std::tuple.
	template<typename T>
		requires::std::is_trivial_v<T>
	class DECLSPEC_EMPTY_BASES CommandSignature<T> : public CommandSignature<void>
	{
		using type = T;

		CommandSignature(Device& device, ::std::shared_ptr<RootSignature> rso,
			::std::vector<::D3D12_INDIRECT_ARGUMENT_DESC> const& args, ::UINT nodeMask = 0u)
			: CommandSignature<void>{ device, rso, args, nodeMask }
		{}
		CommandSignature(Device& device, ::std::vector<::D3D12_INDIRECT_ARGUMENT_DESC> const& args, ::UINT nodeMask = 0u)
			: CommandSignature<void>{ device, args, nodeMask }
		{}
	};

#else if _HAS_CXX17
	// This template is reserved for inner usage.
	//template<typename...Ts>
	//	requires((::std::is_trivial_v<Ts>) &&...)
	//class DECLSPEC_EMPTY_BASES CommandSignature<::std::tuple<Ts...>> 
	//	: public inner::ShareObject<CommandSignature<::std::tuple<Ts...>>>
	//	, public inner::DeviceChild
	//	/*, public inner::MultiNodeObject*/
	//{
	//	
	//private:
	//	static constexpr::std::array<::UINT, sizeof...(Ts)> m_SizeOfBlock{};
	//	static constexpr::std::array<::UINT, sizeof...(Ts)> m_OffsetOfBlock{};
	//};
#endif 

	class CommandBundle
	{

	};

#undef DIA_
}