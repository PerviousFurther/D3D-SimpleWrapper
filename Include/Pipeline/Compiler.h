#pragma once

#include <..\Vendor\DXC\inc\dxcapi.h>

namespace twen::Compilers
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

namespace twen::Compilers
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

	// The class try to represent an dxc.exe. Ordering to make compiling shader easier.
	class ShaderCompiler
	{
		static constexpr auto s_KeyMacro				= L"-D";
		static constexpr auto s_DisableOptimize			= L"-Od";
		static constexpr auto s_KeyEntryName			= L"-E";
		static constexpr auto s_KeyDebugFilePath		= L"-Fd";
		static constexpr auto s_KeyOutputPath			= L"-Fo";
		static constexpr auto s_EnableBackwardCompact	= L"-Gec";
		static constexpr auto s_KeyHlslVersion			= L"-HV";
		static constexpr auto s_KeyTarget				= L"-T";
		static constexpr auto s_DisableValidation		= L"-Vd";
		static constexpr auto s_EnableDebugInfomation	= L"-Zi";
		static constexpr auto s_MatrixColMajor			= L"-Zpc";
		static constexpr auto s_MatrixRowMajor			= L"-Zpr";
		static constexpr auto s_EnableHashOutput		= L"-Fsh";
		static constexpr auto s_EnableSlimDbg			= L"-Zs";
		static constexpr auto s_EnableSeperateReflection= L"-Qstrip_reflect";
	public:
		ShaderCompiler(::std::wstring_view outputDirectory = L"shaders")
			: m_RequiresKind{ DXC_OUT_OBJECT, DXC_OUT_REFLECTION }, m_Settings{ s_EnableSeperateReflection }
		{
			::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_Utils.Put()));
			::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_Compiler.Put()));
			::DxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(m_Validator.Put()));

			m_Parameters.emplace(s_KeyOutputPath, outputDirectory);
		#if D3D12_MODEL_DEBUG
			m_Settings.emplace(s_EnableDebugInfomation);
			m_Parameters.emplace(s_KeyDebugFilePath, outputDirectory);
			m_RequiresKind.emplace(DXC_OUT_PDB);
		#endif
		}
		HRESULT Write(::std::wstring_view path) 
		{
			m_Status = m_Utils->LoadFile(path.data(), nullptr, m_FileBlob.Put());
			return m_Status;
		}
	public:
		inline ShaderCompiler& AddKind(::DXC_OUT_KIND kind) 
		{
			m_RequiresKind.emplace(kind);
			return *this;
		}
		inline ShaderCompiler& Vaildation(bool enable)
		{
			APPEND_SETTING(!enable, s_DisableValidation);
		}
		inline ShaderCompiler& Macro(::std::wstring_view macro)
		{
			APPEND_PARAM(macro.empty(), s_KeyMacro, macro);
		}
		inline ShaderCompiler& HlslVersion(::std::wstring_view version)
		{
			assert(version.find(L"[a-zA-Z]") == version.npos && "");
			APPEND_PARAM(version.empty(), s_KeyHlslVersion, version);
		}
		inline ShaderCompiler& Optimize(bool enable) 
		{
			APPEND_SETTING(!enable, s_DisableOptimize);
		}
		// Express nullptr to use default include handler.
		inline auto& Include(ComPtr<::IDxcIncludeHandler> const& handler) 
		{
			if (handler) m_Include = handler;
			else m_Utils->CreateDefaultIncludeHandler(m_Include.Put());
			return *this;
		}
		inline auto& EntryName(::std::wstring_view name) 
		{
			m_EntryName = name;
			return *this;
		}
		ShaderCompiler& MatrixRowMajor(bool enable);

		::HRESULT Status() const { return m_Status; }
	public:
		::std::unordered_map<::DXC_OUT_KIND, ComPtr<::IDxcBlob>> 
			Compile(::D3D12_SHADER_VERSION_TYPE type, ::std::string& message);

		inline ComPtr<::ID3D12ShaderReflection> CreateReflection(::std::unordered_map<::DXC_OUT_KIND, ComPtr<::IDxcBlob>>& results);

		template<typename T>
		inline ComPtr<T> Create(::std::unordered_map<::DXC_OUT_KIND, ComPtr<::IDxcBlob>>& result);
	private:
		template<typename T>
		inline static DxcBuffer BlobToBuffer(ComPtr<T> blob);

		inline static ComPtr<::IDxcBlob> GetResult(::DXC_OUT_KIND kind, ComPtr<::IDxcResult> result, ComPtr<::IDxcBlobUtf16>* info);
	private:
		ComPtr<::IDxcUtils>		m_Utils;
		ComPtr<::IDxcCompiler3> m_Compiler;
		ComPtr<::IDxcValidator> m_Validator;

		ComPtr<::IDxcBlobEncoding> m_FileBlob;
		ComPtr<::IDxcIncludeHandler> m_Include;

		::std::wstring m_EntryName{};

		::std::unordered_set<::std::wstring_view> m_Settings;
		::std::unordered_map<::std::wstring_view, ::std::wstring> m_Parameters;

		::std::unordered_set<::DXC_OUT_KIND> m_RequiresKind;
		::HRESULT m_Status{ S_OK };
	};

#undef APPEND_SETTING
#undef APPEND_PARAM

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

	// This function assume the result always contain specfied output.
	inline ComPtr<::IDxcBlob> ShaderCompiler::GetResult(
		::DXC_OUT_KIND kind, ComPtr<::IDxcResult> result, ComPtr<::IDxcBlobUtf16>* info)
	{
		ComPtr<::IDxcBlob> ret;
		result->GetOutput(kind, IID_PPV_ARGS(ret.Put()), info ? info->Put() : nullptr);

		return ret;
	}

#define FAILED_LAMBDA(result) \
		if(FAILED(m_Status))\
		{\
			ComPtr<::IDxcBlobEncoding> encoding;\
			result->GetErrorBuffer(encoding.Put());\
			message = static_cast<const char*>(encoding ? encoding->GetBufferPointer() : "Unknown error.");\
			return {};\
		}\

	inline::std::unordered_map<::DXC_OUT_KIND, ComPtr<::IDxcBlob>> 
		ShaderCompiler::Compile(::D3D12_SHADER_VERSION_TYPE type, ::std::string& message)
	{
		::std::vector<LPCWSTR> params(m_Parameters.size() * 2u + m_Settings.size() + 4u);
		auto targetName = ShaderTypeLowerCase(type) + L"_6_1";
		//
		// Start push compile option.
		//
		auto it = params.begin(); 
		// Add compile target.
		*it++ = s_KeyTarget, *it++ = targetName.data();
		// Add entry point name
		*it++ = s_KeyEntryName, *it++ = m_EntryName.empty() ? ShaderTypeToEntryName(type) : m_EntryName.data();
		// Add optional parameter.
		for (auto const& [key, param] : m_Parameters)
			*it++ = key.data(), *it++ = param.data();
		for (auto const& setting : m_Settings)
			*it++ = setting.data();

		MODEL_ASSERT(it == params.end(), "Omitted some value.");

		::BOOL knownCodePage{ false };
		::UINT codePage{ DXC_CP_ACP };
		m_FileBlob->GetEncoding(&knownCodePage, &codePage); // obtain code page.

		ComPtr<::IDxcResult> result;
		DxcBuffer buffer{ m_FileBlob->GetBufferPointer(), m_FileBlob->GetBufferSize(), codePage };
		m_Compiler->Compile(&buffer, params.data(), static_cast<::UINT32>(params.size()), 
			m_Include.Get(), IID_PPV_ARGS(result.Put()));

		result->GetStatus(&m_Status);
		FAILED_LAMBDA(result);

		// Obtain result so we can talk about it.
		::std::unordered_map<::DXC_OUT_KIND, ComPtr<::IDxcBlob>> ret;
		for (auto const& kind : m_RequiresKind) 
			if (result->HasOutput(kind))
				ret.emplace(kind, GetResult(kind, result, nullptr));
			else { MODEL_ASSERT(false, "Requires kind have no output."); }

		auto &blob = ret.at(::DXC_OUT_OBJECT);
		
		// Dont forget to vaildate.
		ComPtr<::IDxcOperationResult> vaildateResult;
		m_Validator->Validate(blob.Get(), DxcValidatorFlags_Default, vaildateResult.Put());

		vaildateResult->GetStatus(&m_Status);
		FAILED_LAMBDA(vaildateResult);

		return ret;
	}

#undef FAILED_LAMBDA

	template<typename T>
	inline DxcBuffer ShaderCompiler::BlobToBuffer(ComPtr<T> blob)
	{
		DxcBuffer result{ blob->GetBufferPointer(), blob->GetBufferSize() };
		if constexpr (requires(BOOL * kn, UINT * pg) { blob->GetEncoding(kn, pg); })
		{
			::BOOL known{};
			::UINT page{};
			blob->GetEncoding(&known, &page);
			result.Encoding = known ? page : 0u;
		}
		return result;
	}

	inline ComPtr<::ID3D12ShaderReflection> ShaderCompiler::CreateReflection(::std::unordered_map<::DXC_OUT_KIND, ComPtr<::IDxcBlob>>& results)
	{
		assert(results.contains(::DXC_OUT_REFLECTION) && "Try get reflection with no reflections output.");

		ComPtr<::ID3D12ShaderReflection> result;
		DxcBuffer buffer{ this->BlobToBuffer(results.at(::DXC_OUT_REFLECTION)) };
		m_Status = m_Utils->CreateReflection(&buffer, IID_PPV_ARGS(result.Put()));

		return result;
	}




}