#pragma once

#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")
#include <d3d12shader.h>

//namespace core 
//{
//	using shader_binding_t =::D3D12_SHADER_BYTECODE(::D3D12_GRAPHICS_PIPELINE_STATE_DESC::*);
//
//	static inline shader_binding_t ShaderTypeToBinding(D3D12_SHADER_VERSION_TYPE type)
//	{
//		switch (type)
//		{
//		case D3D12_SHVER_PIXEL_SHADER: return PIPE_SHADER_CODE(PS);
//		case D3D12_SHVER_VERTEX_SHADER: return PIPE_SHADER_CODE(VS);
//		case D3D12_SHVER_GEOMETRY_SHADER: return PIPE_SHADER_CODE(GS);
//		case D3D12_SHVER_HULL_SHADER: return PIPE_SHADER_CODE(HS);
//		case D3D12_SHVER_DOMAIN_SHADER: return PIPE_SHADER_CODE(DS);
//		default:assert(!"Unknown shader type or try expressing compute shader type to get graphics shader binding.");
//			return nullptr;
//		}
//	}
//
namespace twen 
{
	// Consume an file is an pipeline, thus every entry point name is fixed.
	static inline LPCWSTR ShaderTypeToEntryName(D3D12_SHADER_VERSION_TYPE type)
	{
		switch (type)
		{
		case D3D12_SHVER_PIXEL_SHADER: return L"PSmain";
		case D3D12_SHVER_VERTEX_SHADER: return L"VSmain";
		case D3D12_SHVER_GEOMETRY_SHADER: return L"GSmain";
		case D3D12_SHVER_HULL_SHADER: return L"HSmain";
		case D3D12_SHVER_DOMAIN_SHADER: return L"DSmain";
		case D3D12_SHVER_COMPUTE_SHADER: return L"CSmain";
		default:assert(!"Specified shader type doesn't have EntryName.");
			return L"";
		}
	}

	static inline::std::wstring ShaderTypeLowerCase(D3D12_SHADER_VERSION_TYPE type)
	{
		switch (type)
		{
		case D3D12_SHVER_PIXEL_SHADER: return L"ps";
		case D3D12_SHVER_VERTEX_SHADER: return L"vs";
		case D3D12_SHVER_GEOMETRY_SHADER: return L"gs";
		case D3D12_SHVER_HULL_SHADER: return L"hs";
		case D3D12_SHVER_DOMAIN_SHADER: return L"ds";
		case D3D12_SHVER_COMPUTE_SHADER: return L"cs";
		default:assert(!"Unknown shader type."); return L"";
		}
	}
}
//
//	static inline constexpr::DXGI_FORMAT GetSignatureParameterFormat(::D3D_REGISTER_COMPONENT_TYPE type, ::std::uint8_t mask)
//	{
//		switch (type)
//		{
//		case::D3D_REGISTER_COMPONENT_FLOAT32:
//			switch (mask)
//			{
//			case 0x1:return::DXGI_FORMAT_R32_FLOAT;
//			case 0x3:return::DXGI_FORMAT_R32G32_FLOAT;
//			case 0x7:return::DXGI_FORMAT_R32G32B32_FLOAT;
//			case 0xf:return::DXGI_FORMAT_R32G32B32A32_FLOAT;
//			default:assert(!"Unkwn float count.");
//				return::DXGI_FORMAT_UNKNOWN;
//			}
//		case::D3D_REGISTER_COMPONENT_UINT32:
//			switch (mask)
//			{
//			case 0x1:return::DXGI_FORMAT_R32_UINT;
//			case 0x3:return::DXGI_FORMAT_R32G32_UINT;
//			case 0x7:return::DXGI_FORMAT_R32G32B32_UINT;
//			case 0xf:return::DXGI_FORMAT_R32G32B32A32_UINT;
//			default:assert(!"Unkwn uint count.");
//				return::DXGI_FORMAT_UNKNOWN;
//			}
//		case::D3D_REGISTER_COMPONENT_SINT32:
//			switch (mask)
//			{
//			case 0x1:return::DXGI_FORMAT_R32_SINT;
//			case 0x3:return::DXGI_FORMAT_R32G32_SINT;
//			case 0x7:return::DXGI_FORMAT_R32G32B32_SINT;
//			case 0xf:return::DXGI_FORMAT_R32G32B32A32_SINT;
//			default:assert(!"Unkwn sint count.");
//				return::DXGI_FORMAT_UNKNOWN;
//			}
//		default:assert(!"Unkwn component type."); return::DXGI_FORMAT_UNKNOWN;
//		}
//	}
//
//	static inline constexpr::D3D12_RESOURCE_DIMENSION GetDimensionByReflectedDimension(::D3D_SRV_DIMENSION dimension)
//	{
//		switch (dimension)
//		{
//		case::D3D12_RESOURCE_DIMENSION_BUFFER:
//			return::D3D12_RESOURCE_DIMENSION_BUFFER;
//
//		case::D3D12_SRV_DIMENSION_TEXTURE1D:
//		case::D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
//			return::D3D12_RESOURCE_DIMENSION_TEXTURE1D;
//
//		case::D3D12_SRV_DIMENSION_TEXTURE2D:
//		case::D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
//			/*UNSURE*/
//		case::D3D12_SRV_DIMENSION_TEXTURE2DMS:
//		case::D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
//		case::D3D12_SRV_DIMENSION_TEXTURECUBE:
//		case::D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
//			return::D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//
//		case::D3D12_SRV_DIMENSION_TEXTURE3D:
//			return::D3D12_RESOURCE_DIMENSION_TEXTURE3D;
//
//		case::D3D_SRV_DIMENSION_UNKNOWN: // for buffer is ok.
//		default:return::D3D12_RESOURCE_DIMENSION_UNKNOWN;
//		}
//	}
//
//	static inline constexpr::D3D12_DESCRIPTOR_RANGE_TYPE ShaderInputTypeToDescriptorRangeType(::D3D_SHADER_INPUT_TYPE type)
//	{
//		switch (type)
//		{
//		case D3D_SIT_CBUFFER:
//		case D3D_SIT_STRUCTURED:
//		case D3D_SIT_BYTEADDRESS:
//			return::D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
//
//		case D3D_SIT_TBUFFER:
//		case D3D_SIT_TEXTURE:
//			return::D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
//
//		case D3D_SIT_SAMPLER:
//			return::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
//
//		case D3D_SIT_UAV_RWTYPED:
//		case D3D_SIT_UAV_RWSTRUCTURED:
//		case D3D_SIT_UAV_RWBYTEADDRESS:
//		case D3D_SIT_UAV_APPEND_STRUCTURED:
//		case D3D_SIT_UAV_CONSUME_STRUCTURED:
//		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
//			return::D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
//
//		default:assert(!"Unknown shader input type.");
//			return::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
//		}
//	}
//
//	static inline constexpr::D3D12_ROOT_PARAMETER_TYPE ShaderInputTypeToDescriptorType(::D3D_SHADER_INPUT_TYPE type)
//	{
//		switch (type)
//		{
//		case::D3D_SIT_CBUFFER:
//			return::D3D12_ROOT_PARAMETER_TYPE_CBV;
//
//		case::D3D_SIT_STRUCTURED:
//		case::D3D_SIT_BYTEADDRESS:
//		case::D3D_SIT_SAMPLER:
//		case::D3D_SIT_TEXTURE:
//		case::D3D_SIT_UAV_RWTYPED:
//		case::D3D_SIT_UAV_RWSTRUCTURED:
//		case::D3D_SIT_UAV_RWBYTEADDRESS:
//		case::D3D_SIT_UAV_APPEND_STRUCTURED:
//		case::D3D_SIT_UAV_CONSUME_STRUCTURED:
//		case::D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
//			return::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
//
//		case::D3D_SIT_TBUFFER:
//			return::D3D12_ROOT_PARAMETER_TYPE_SRV;
//
//		default:assert(!"Unknown shader input type.");
//			return::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
//		}
//	}
//}

namespace twen 
{
	// condition is false then erase setting.
#define APPEND_SETTING(condition, name)\
if (condition)							\
m_Settings.emplace(name);				 \
else {									 \
if (m_Settings.contains(name))			 \
m_Settings.erase(name);					 \
}										 \
return *this

	// condition is true then erase param.
#define APPEND_PARAM(condition, key, param)\
if (condition)								\
if (m_Parameters.contains(key))				\
m_Parameters.erase(key);					\
else m_Parameters.emplace(key, param);		\
return *this

	class ShaderCompiler 
	{
		static constexpr auto s_KeyMacro = L"-D";
		static constexpr auto s_DisableOptimize = L"-Od";
		static constexpr auto s_KeyEntryName = L"-E";
		static constexpr auto s_KeyDebugFilePath = L"-Fd";
		static constexpr auto s_KeyOutputPath = L"-Fo";
		static constexpr auto s_EnableBackwardCompact = L"-Gec";
		static constexpr auto s_KeyHlslVersion = L"-HV";
		static constexpr auto s_KeyTarget = L"-T";
		static constexpr auto s_DisableValidation = L"-Vd";
		static constexpr auto s_EnableDebugInfomation = L"-Zi";
		static constexpr auto s_MatrixColMajor = L"-Zpc";
		static constexpr auto s_MatrixRowMajor = L"-Zpr";
		static constexpr auto s_EnableHashOutput = L"-Fsh";
		static constexpr auto s_EnableSlimDbg = L"-Zs";
		static constexpr auto s_EnableSeperateReflection = L"-Qstrip_reflect";
	public:
		ShaderCompiler(::std::wstring_view path, ::std::wstring_view outputDirectory = L"shaders")
			: m_RequiresKind{ DXC_OUT_OBJECT }, m_Settings{ s_EnableSeperateReflection }
		{
			::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_Utils.put()));
			::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_Compiler.put()));
			::DxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(m_Validator.put()));

			m_Status = m_Utils->LoadFile(path.data(), nullptr, m_FileBlob.put());
			assert(SUCCEEDED(m_Status) && "Error on load file.");
			
			assert(!outputDirectory.empty() && "Ouput directory cannot be null.");
			m_Parameters.emplace(s_KeyOutputPath, outputDirectory);
		#if D3D12_MODEL_DEBUG
			m_Settings.emplace(s_EnableDebugInfomation);
			m_Parameters.emplace(s_KeyDebugFilePath, outputDirectory);
			m_RequiresKind.emplace(DXC_OUT_PDB);
		#endif
		}
		FORCEINLINE ShaderCompiler& AddKind(::DXC_OUT_KIND kind) 
		{
			m_RequiresKind.emplace(kind);
			return *this;
		}
		FORCEINLINE ShaderCompiler& Vaildation(bool enable)
		{
			APPEND_SETTING(!enable, s_DisableValidation);
		}
		FORCEINLINE ShaderCompiler& Macro(::std::wstring_view macro)
		{
			APPEND_PARAM(macro.empty(), s_KeyMacro, macro);
		}
		FORCEINLINE ShaderCompiler& HlslVersion(::std::wstring_view version)
		{
			assert(version.find(L"[a-zA-Z]") == version.npos && "");
			APPEND_PARAM(version.empty(), s_KeyHlslVersion, version);
		}
		FORCEINLINE ShaderCompiler& Optimize(bool enable) 
		{
			APPEND_SETTING(!enable, s_DisableOptimize);
		}
		// Express nullptr to use default include handler.
		FORCEINLINE auto& Include(ComPtr<::IDxcIncludeHandler> const& handler) 
		{
			if (handler) m_Include = handler;
			else m_Utils->CreateDefaultIncludeHandler(m_Include.put());
			return *this;
		}
		ShaderCompiler& MatrixRowMajor(bool enable);
	public:
		::std::unordered_map<::DXC_OUT_KIND, ComPtr<::IDxcBlob>> 
			Compile(::D3D12_SHADER_VERSION_TYPE type, ::std::string& message);
		::HRESULT GetStatus() const { return m_Status; }
	private:
		ComPtr<::IDxcBlob> GetResult(::DXC_OUT_KIND kind, ComPtr<::IDxcResult> result, ComPtr<::IDxcBlobUtf16>* info);
	private:
		ComPtr<::IDxcUtils> m_Utils;
		ComPtr<::IDxcCompiler3> m_Compiler;
		ComPtr<::IDxcValidator> m_Validator;

		::HRESULT m_Status{ S_OK };

		ComPtr<::IDxcBlobEncoding> m_FileBlob;

		ComPtr<::IDxcIncludeHandler> m_Include;
		::std::unordered_set<::std::wstring_view> m_Settings;
		::std::unordered_map<::std::wstring_view, ::std::wstring> m_Parameters;

		::std::unordered_set<::DXC_OUT_KIND> m_RequiresKind;
	};

	inline ShaderCompiler& ShaderCompiler::MatrixRowMajor(bool enable)
	{
		if (enable)
		{
			if (m_Settings.contains(s_MatrixColMajor))
				m_Settings.erase(s_MatrixColMajor);
			m_Settings.emplace(s_MatrixRowMajor);
		} else {
			if (m_Settings.contains(s_MatrixRowMajor))
				m_Settings.erase(s_MatrixRowMajor);
			m_Settings.emplace(s_MatrixColMajor);
		}
		return *this;
	}

	inline ComPtr<::IDxcBlob> ShaderCompiler::GetResult(::DXC_OUT_KIND kind, 
		ComPtr<::IDxcResult> result, ComPtr<::IDxcBlobUtf16>* info)
	{
		ComPtr<::IDxcBlob> ret;
		m_Status = result->GetOutput(kind, IID_PPV_ARGS(ret.put()), info ? info->put() : nullptr);
		return ret;
	}
	
	inline::std::unordered_map<::DXC_OUT_KIND, ComPtr<::IDxcBlob>> 
		ShaderCompiler::Compile(::D3D12_SHADER_VERSION_TYPE type, ::std::string& message)
	{
		::std::vector<LPCWSTR> params(m_Parameters.size() * 2u + m_Settings.size() + 4u);
		auto targetName = ShaderTypeLowerCase(type) + L"_6_1";

		auto it = params.begin();						*it++ = s_KeyTarget, *it++ = targetName.data();
														*it++ = s_KeyEntryName, *it++ = ShaderTypeToEntryName(type);
		for (auto const& [key, param] : m_Parameters)	*it++ = key.data(), *it++ = param.data();
		for (auto const& setting : m_Settings)			*it++ = setting.data();
		assert(it == params.end());

		::BOOL knownCodePage{ false };
		::UINT codePage{ DXC_CP_ACP };
		m_FileBlob->GetEncoding(&knownCodePage, &codePage);

		ComPtr<::IDxcResult> result;
		DxcBuffer buffer{ m_FileBlob->GetBufferPointer(), m_FileBlob->GetBufferSize(), codePage };
		m_Compiler->Compile(&buffer, params.data(), static_cast<::UINT32>(params.size()), 
			m_Include.get(), IID_PPV_ARGS(result.put()));

		result->GetStatus(&m_Status);
		if (FAILED(m_Status)) 
		{
			if (result->HasOutput(DXC_OUT_ERRORS))
			{
				ComPtr<::IDxcBlobEncoding> error;
				result->GetErrorBuffer(error.put());

				message = static_cast<const char*>(error->GetBufferPointer());
			} else message = "Inner error.";

			return {};
		}

		::std::unordered_map<::DXC_OUT_KIND, ComPtr<::IDxcBlob>> ret;
		for (auto const& kind : m_RequiresKind) 
			if (result->HasOutput(kind))
				ret.emplace(kind, GetResult(kind, result, nullptr));
			else { assert(!"Requires kind have no output."); }

		auto &blob = ret.at(::DXC_OUT_OBJECT);

		ComPtr<::IDxcOperationResult> vaildateResult;
		m_Validator->Validate(blob.get(), DxcValidatorFlags_Default, vaildateResult.put());

		vaildateResult->GetStatus(&m_Status);
		if(FAILED(m_Status))
		{
			ComPtr<::IDxcBlobEncoding> encoding;
			vaildateResult->GetErrorBuffer(encoding.put());

			message = static_cast<const char*>(encoding->GetBufferPointer());

			return {};
		}
		return ret;
	}

}