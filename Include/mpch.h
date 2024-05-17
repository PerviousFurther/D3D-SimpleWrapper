#pragma once

#ifdef USE_MODULE
	#define TWEN_EXPORT export
#else
	#include <set>
	#include <unordered_set>
	
	#include <mdspan>
	#include <ranges>
	
	#include <cassert>
	
	#if defined(D3D12_MODEL_USE_WINRT)
		#include <vendor\base_4.h>
	#else
		#include <wrl\implements.h>
	
		#include <format>
		#include <string>
		#include <memory>
		#include <map>
		#include <unordered_map>
		#include <concepts>
		#include <list>
		#include <assert.h>
	#endif

	#define TWEN_EXPORT 

#endif // !USE_MODULE

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d12shader.h>



#if defined(_DEBUG) || defined(ENABLE_D3D12_MODEL_DEBUG)
	#define D3D12_MODEL_DEBUG 1
#else
	#define D3D12_MODEL_DEBUG 0
#endif

#if defined(_MSC_VER) || defined(__CLANG__)
	#define DECLSPEC_EMPTY_BASES __declspec(empty_bases)

	#if D3D12_MODEL_DEBUG
		#define MODEL_ASSERT(condition, description) ((!!(condition)) ? (void)(0) : _wassert(L"[Inner Error]: " description, __FILEW__, __LINE__))
	#else
		#define MODEL_ASSERT(condition, description) (void)(0)
	#endif
#else 
	#define DECLSPEC_EMPTY_BASES 
	#define MODEL_ASSERT(condtion, description) assert((condition) && description)
#endif // 

#if D3D12_MODEL_DEBUG
	#include <dxgidebug.h>

	#define DEBUG_OPERATION(x) x
	#include "Misc\Debug.h"
#else
	#define DEBUG_OPERATION(x) 
#endif 

#include "Misc\Common.h"