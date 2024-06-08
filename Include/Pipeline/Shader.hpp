#pragma once

namespace twen::Shaders 
{
	using Type = ::D3D12_SHADER_VERSION_TYPE;

	// Duplicate simply.
	struct Input
	{
		::std::string			Name;

		::UINT					BindCount : 8;
		::UINT					BindPoint : 8;
		::UINT					Space : 8;

		::D3D_SHADER_INPUT_TYPE	Type : 8;

		operator::std::string const& () const { return Name; }
		operator::UINT32() const { return (BindCount << 16) | (BindPoint << 8) | (Space); }

		static Input Create(::D3D12_SHADER_INPUT_BIND_DESC const& desc)
		{
			return Input
			{
				desc.Name,
				desc.BindCount,
				desc.BindPoint,
				desc.Space,
				desc.Type,
			};
		}
	};

	// Reserved.
	struct Signature
	{
		constexpr Signature() = default;
		Signature(::D3D12_SIGNATURE_PARAMETER_DESC const& desc)
			: SemanticName{ desc.SemanticName }
			, SemanticIndex{ desc.SemanticIndex }
			, ComponentFormat{ Utils::MakeFormat(desc.ComponentType, desc.Mask) }
		{}
		::std::string	SemanticName;
		::UINT			SemanticIndex;
		::DXGI_FORMAT	ComponentFormat;
	};
}

TWEN_EXPORT namespace twen 
{
	struct VertexShader;
	struct AmplicationShader;
	
	struct ShaderDesc : D3D12_SHADER_DESC
	{
		ShaderDesc() = default;
		ShaderDesc(ComPtr<::ID3D12ShaderReflection> reflection, ComPtr<::ID3DBlob> blob)
			: D3D12_SHADER_DESC{}
			, Blob{blob}
		{
			assert(blob&&"Empty blob.");
			assert(reflection && "Empty reflection");

			reflection->GetDesc(this);
			reflection->GetThreadGroupSize(&X, &Y, &Z);

			for (auto i{ 0u }; i < InputParameters; ++i) 
			{
				::D3D12_SIGNATURE_PARAMETER_DESC temp;
				reflection->GetInputParameterDesc(i, &temp);
				if (!temp.SystemValueType) 
					InputDesc.emplace_back(temp);
			}

			for (auto i{ 0u }; i < OutputParameters; ++i)
			{
				::D3D12_SIGNATURE_PARAMETER_DESC temp;
				reflection->GetOutputParameterDesc(i, &temp);
				if (!temp.SystemValueType) 
					OutputDesc.emplace_back(temp);
			}

			for (auto i{ 0u }; i < BoundResources; ++i)
			{
				::D3D12_SHADER_INPUT_BIND_DESC temp;
				reflection->GetResourceBindingDesc(i, &temp);
				BindDescs.emplace_back(Shaders::Input::Create(temp));
			}
		}

		ComPtr<::ID3DBlob> Blob;

		::UINT X, Y, Z;

		::std::vector<Shaders::Signature> InputDesc;
		::std::vector<Shaders::Signature> OutputDesc;
		::std::vector<Shaders::Input>	  BindDescs;
	};
	
	struct ShaderNode : inner::ShareObject<ShaderNode>
	{
	public:
		ShaderNode(ShaderDesc const& desc, Shaders::Type type, ::D3D12_ROOT_SIGNATURE_FLAGS requirement =::D3D12_ROOT_SIGNATURE_FLAG_NONE)
			: Type{ type }
			, Requirement{requirement}
			, NumConstantBuffer{desc.ConstantBuffers}
			, InputSignatures{desc.InputDesc}
			, OutputSignatures{desc.OutputDesc}
			, BindingDescs{desc.BindDescs}
			, m_Blob{desc.Blob} {
			// temporary.
			for (auto const& input : BindingDescs) 
				m_NameMap.emplace(input.Name, &input);
		}
	public:
		Shaders::Signature const& InputSignature(::UINT index) const
		{ 
			assert(index < InputSignatures.size() && "Out of range.");
			return InputSignatures.at(index); 
		}
		Shaders::Signature const& OuputSignature(::UINT index) const
		{
			assert(index < InputSignatures.size() && "Out of range.");
			return InputSignatures.at(index);
		}
		Shaders::Input const& BindingDesc(::UINT index) const
		{ 
			assert(index < BindingDescs.size() && "Out of range.");
			return BindingDescs.at(index); 
		}
		Shaders::Input const& BindingDesc(::std::string_view name) const
		{
			MODEL_ASSERT(m_NameMap.contains(name), "Out of range.");
			return *m_NameMap.at(name);
		}

		::D3D12_SHADER_BYTECODE Bytecode() const { return { m_Blob->GetBufferPointer(), m_Blob->GetBufferSize() }; }

		void Bytecode(::D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) const 
		{
			MODEL_ASSERT(/*Type !=::D3D12_SHVER_AMPLIFICATION_SHADER && Type != ::D3D12_SHVER_MESH_SHADER &&*/ Type !=::D3D12_SHVER_COMPUTE_SHADER, 
				"Mismatch shader type with pipeline desc.");
			desc.*Utils::ShaderTypeToBindPoint(Type) = { m_Blob->GetBufferPointer(), m_Blob->GetBufferSize() }; 
		}
		void Bytecode(::D3D12_COMPUTE_PIPELINE_STATE_DESC& desc) const 
		{ 
			MODEL_ASSERT(/*Type ==::D3D12_SHVER_AMPLIFICATION_SHADER && Type ==::D3D12_SHVER_MESH_SHADER && */Type ==::D3D12_SHVER_COMPUTE_SHADER, 
				"Mismatch shader type with pipeline desc.");
			desc.CS = { m_Blob->GetBufferPointer(), m_Blob->GetBufferSize() };
		}

		ShaderNode* Next();
	public:
		const Shaders::Type Type;

		const::D3D12_ROOT_SIGNATURE_FLAGS Requirement;

		const::UINT NumConstantBuffer;

		const::std::vector<Shaders::Signature> InputSignatures;
		const::std::vector<Shaders::Signature> OutputSignatures;
		const::std::vector<Shaders::Input>	   BindingDescs;
	private:
		::std::unordered_map<::std::string_view, Shaders::Input const*> m_NameMap;

		ComPtr<::ID3DBlob> m_Blob;
	};

	struct VertexShader : ShaderNode
	{
	public:
		static constexpr auto Type{::D3D12_SHVER_VERTEX_SHADER};
		static constexpr::D3D12_ROOT_SIGNATURE_FLAGS 
			RootSignatureRequirement{ ::D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
	public:
		VertexShader(ShaderDesc const& desc) : ShaderNode{desc, Type, RootSignatureRequirement} {}

		// This method will take ownership of other shader.
		// So it is same to release other shader node after link.
		// but it is not safe for switch linking other shader.
		void Link(::std::shared_ptr<ShaderNode> next) 
		{ 
			MODEL_ASSERT(m_Next.get() != this, "Ring linking is not allowed."); 
			m_Next = next; 
		}
		ShaderNode* Next() { return m_Next.get(); }
	private:
		::std::shared_ptr<ShaderNode> m_Next;
	};

	struct PixelShader : ShaderNode
	{
	public:
		static constexpr auto Type{::D3D12_SHVER_PIXEL_SHADER};
	public:
		PixelShader(ShaderDesc const& desc) : ShaderNode{desc, Type, } {}

		void Link(::std::shared_ptr<ShaderNode> prev) { Prev = prev; }
	private:
		::std::shared_ptr<ShaderNode> Prev;
	};

	// will soon deprecated. 
	struct GeometryShader : ShaderNode
	{
	public:
		static constexpr auto Type{::D3D12_SHVER_GEOMETRY_SHADER};

		static constexpr::D3D12_ROOT_SIGNATURE_FLAGS RootSignatureRequirement{::D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT};
	public:
		GeometryShader(ShaderDesc const& desc) 
			: ShaderNode{ desc, Type, RootSignatureRequirement }
			, OutputTopology{ desc.GSOutputTopology }
			, MaxOutputVertexCount{ desc.GSMaxOutputVertexCount }
			, InputPrimtive{ desc.InputPrimitive } {}

		void LinkPrev(::std::shared_ptr<ShaderNode> prev) 
		{ 
			MODEL_ASSERT(prev->Type !=::D3D12_SHVER_PIXEL_SHADER, "Link pixel shader as before is not allowed."); 
			m_Prev = prev; 
		}
		void LinkNext(::std::shared_ptr<PixelShader> next){ m_Next = next; }
		ShaderNode* Next() { return m_Next.get(); }
	public:
		const::D3D_PRIMITIVE InputPrimtive;
		const::UINT MaxOutputVertexCount;
		const::D3D_PRIMITIVE_TOPOLOGY OutputTopology;
	private:
		::std::shared_ptr<ShaderNode> m_Prev;
		::std::shared_ptr<PixelShader>m_Next;
	};

	struct ComputeShader : ShaderNode 
	{
	private:
		struct ThreadGroupInfo { ::UINT X, Y, Z; };
	public:
		static constexpr auto Type{::D3D12_SHVER_COMPUTE_SHADER};
	public:
		ComputeShader(ShaderDesc const& desc) 
			: ShaderNode{ desc, Type }
			, ThreadGroup{ desc.X, desc.Y, desc.Z } {}
	public:
		const ThreadGroupInfo ThreadGroup;
	};

	inline ShaderNode* ShaderNode::Next() 
	{
		switch (Type)
		{
		case::D3D12_SHVER_VERTEX_SHADER:
			return reinterpret_cast<VertexShader*>(this)->Next();
		case::D3D12_SHVER_GEOMETRY_SHADER:
			return reinterpret_cast<GeometryShader*>(this)->Next();
		default:return nullptr;
		}
	}
}
#include "Compiler.h"

TWEN_EXPORT namespace twen 
{
	class ShaderManager : public::std::enable_shared_from_this<ShaderManager>
	{
	public:
		void ReadFile(::std::wstring_view file) 
		{ 
			if (!m_Compiler) 
				m_Compiler = ::std::make_unique<Compilers::ShaderCompiler>(); 
			m_Compiler->Write(file);
			MODEL_ASSERT(SUCCEEDED(m_Compiler->Status()), "Read file failure.");
		}
		inline::std::unique_ptr<Compilers::ShaderCompiler> DetchCompiler() { return::std::move(m_Compiler); }

		template<inner::congener<ShaderNode> T>
			requires(!::std::is_same_v<ShaderNode, T>)
		inline::std::shared_ptr<T> Obtain(::std::wstring_view entryName = {})
		{
			::std::string message;

			if(!entryName.empty()) m_Compiler->EntryName(entryName);
			auto map{ m_Compiler->Compile(T::Type, message) };

			MODEL_ASSERT(message.empty(), "Error occured.");

			auto reflection{ m_Compiler->CreateReflection(map) };
			ShaderDesc desc{ reflection, map.at(DXC_OUT_OBJECT).As<::ID3DBlob>()};
			
			auto shader{ ::std::make_shared<T>(desc) };
			m_Shaders.emplace(shader);
			return shader;
		}
	private:
		::std::unique_ptr<Compilers::ShaderCompiler> m_Compiler;

		::std::unordered_set<::std::shared_ptr<ShaderNode>> m_Shaders;
	};
}