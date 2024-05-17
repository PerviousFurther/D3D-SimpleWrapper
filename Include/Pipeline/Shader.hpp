#pragma once

namespace twen 
{
	struct VertexShader;
	struct AmplicationShader;
	namespace Descriptors 
	{
		struct Range;
	}
	namespace Shaders 
	{
		using Type =::D3D12_SHADER_VERSION_TYPE;

		// Duplicate simply.
		struct Input 
		{
			static Input Create(::D3D12_SHADER_INPUT_BIND_DESC const& desc) 
			{
				return Input
				{
					nullptr,
					desc.Name,
					desc.Type,
					desc.BindCount,
					desc.BindPoint,
					desc.Space,
					Utils.ShaderInputTypeToDescriptorRangeType(desc.Type),
					nullptr
				};
			}
			
			void Attach(Descriptors::Range& range)&;
			void Detach();

			operator::std::string const&() const { return Name; }
			operator unsigned short() const { return static_cast<::UINT16>((RangeType << 13) | (BindCount << 8) | (BindPoint << 4) | (Space)); }

			Descriptors::Range *BindedRange{nullptr};

			operator Descriptors::Range() const;

			::std::string			Name;
			::D3D_SHADER_INPUT_TYPE	Type;

			::UINT					BindCount : 5; // In shaders part: bound resource count. In context: resource count that have bound to descriptors.
			::UINT					BindPoint : 4; // In shaders part: register. In context: root index. (not range index).
			::UINT					Space : 4;	// In shaders part: register space. In context: offset from descriptor span start in descriptors.
			::D3D12_DESCRIPTOR_RANGE_TYPE RangeType : 3;

			void* Reserved;
		};

		// Duplicate simply.
		struct Signature
		{
			constexpr Signature() = default;
			Signature(::D3D12_SIGNATURE_PARAMETER_DESC const& desc)
				: SemanticName{ desc.SemanticName }
				, SemanticIndex{ desc.SemanticIndex }
				, ComponentFormat{ Utils.MakeFormat(desc.ComponentType, desc.Mask) }
			{}

			::std::string	SemanticName;
			::UINT			SemanticIndex;
			::DXGI_FORMAT	ComponentFormat;
		};

		struct Transformer
		{
			auto operator()(::D3D12_INPUT_CLASSIFICATION inputSlotClass, ::UINT instanceStepRate,
				::UINT slot, ::UINT alignedByte, DXGI_FORMAT optFormat) const
			{
				return::D3D12_INPUT_ELEMENT_DESC
				{
					Signature->SemanticName.data(),
					Signature->SemanticIndex,
					optFormat == ::DXGI_FORMAT_UNKNOWN ? Signature->ComponentFormat : optFormat,
					slot, alignedByte, 
					inputSlotClass, 
					instanceStepRate
				};
			}

			auto operator()(::D3D12_SHADER_VISIBILITY visiblity =::D3D12_SHADER_VISIBILITY_ALL)
			{
				assert(BindingInput->Type !=::D3D_SIT_SAMPLER&& "Sampler is not allow as an root parameter.");
				assert(BindingInput->Type !=::D3D_SIT_TEXTURE && "Texture is not allow as an root parameter.");
				assert(BindingInput->BindCount == 1u && "Using descriptor table for range.");

				::D3D12_ROOT_PARAMETER_TYPE type{ Utils.ShaderInputTypeToRootParameterType(BindingInput->Type) };
				assert(type !=::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
				return::D3D12_ROOT_PARAMETER1
				{ 
					.ParameterType{type},
					.Descriptor{
						BindingInput->BindPoint,
						BindingInput->Space,
						BindingInput->Type == ::D3D_SIT_CBUFFER ?
							::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE | FlagsRoot : FlagsRoot
					},
					.ShaderVisibility{visiblity},
				};
			}

			auto operator()(::UINT offsetInDescriptor)
			{
				auto type{ Utils.ShaderInputTypeToDescriptorRangeType(BindingInput->Type) };
				return::D3D12_DESCRIPTOR_RANGE1
				{
					type,
					BindingInput->BindCount,
					BindingInput->BindPoint,
					BindingInput->Space,
					(type == ::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? ::D3D12_DESCRIPTOR_RANGE_FLAG_NONE :
						type == ::D3D12_DESCRIPTOR_RANGE_TYPE_UAV ? D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE :
						::D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE) | FlagsRange,
					offsetInDescriptor
				};
			}

			Transformer(Signature const& desc) 
				: Signature{&desc} {}
			Transformer(Input const& desc) 
				: BindingInput{&desc} {}

			union
			{
				const Input* const		BindingInput;
				const Signature* const	Signature;
			};

			union 
			{
				::D3D12_DESCRIPTOR_RANGE_FLAGS FlagsRange;
				::D3D12_ROOT_DESCRIPTOR_FLAGS  FlagsRoot;
			};
		};
	}

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
	
	struct ShaderNode : ShareObject<ShaderNode>
	{
	public:
		ShaderNode(ShaderDesc const& desc, Shaders::Type type, ::D3D12_ROOT_SIGNATURE_FLAGS requirement =::D3D12_ROOT_SIGNATURE_FLAG_NONE)
			: Type{ type }
			, Requirement{requirement}
			, NumConstantBuffer{desc.ConstantBuffers}
			, InputSignatures{desc.InputDesc}
			, OutputSignatures{desc.OutputDesc}
			, BindingDescs{desc.BindDescs}
			, m_Blob{desc.Blob} {}
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
		::D3D12_SHADER_BYTECODE Bytecode() const { return { m_Blob->GetBufferPointer(), m_Blob->GetBufferSize() }; }

		void Bytecode(::D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) const 
		{ desc.*Utils.ShaderTypeToBindPoint(Type) = { m_Blob->GetBufferPointer(), m_Blob->GetBufferSize() }; }

		ShaderNode* Next();
	public:
		const Shaders::Type Type;
		const::D3D12_ROOT_SIGNATURE_FLAGS Requirement;
		const::UINT NumConstantBuffer;
		const::std::vector<Shaders::Signature> InputSignatures;
		const::std::vector<Shaders::Signature> OutputSignatures;
		const::std::vector<Shaders::Input>	   BindingDescs;
	private:
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

		void Link(::std::shared_ptr<ShaderNode> next) { assert(m_Next.get() != this); m_Next = next; }
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

		void LinkPrev(::std::shared_ptr<ShaderNode> prev) { assert(prev->Type !=::D3D12_SHVER_PIXEL_SHADER&&"Link pixel shader as before is not allowed."); 
															m_Prev = prev; }
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
namespace twen 
{
	class ShaderManager : public::std::enable_shared_from_this<ShaderManager>
	{
	public:
		void ReadFile(::std::wstring_view file) 
		{ 
			if (!m_Compiler) 
				m_Compiler = ::std::make_unique<Compilers::ShaderCompiler>(); 
			m_Compiler->Read(file);
			assert(SUCCEEDED(m_Compiler->Status()) && "Read file failure.");
		}
		inline::std::unique_ptr<Compilers::ShaderCompiler> DetchCompiler() { return::std::move(m_Compiler); }

		template<inner::congener<ShaderNode> T>
		::std::shared_ptr<T> Obtain() 
		{
			::std::string message;

			auto map{ m_Compiler->Compile(T::Type, message) };

			assert(message.empty() && "Error occured.");

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