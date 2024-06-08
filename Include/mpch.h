#pragma once

#ifdef USE_MODULE
	#define TWEN_EXPORT export
#else
	#include <set>
	#include <unordered_set>
	
	#include <mdspan>
	#include <ranges>
	#include <array>
	#include <queue>
	#include <mutex>
	#include <algorithm>
	#include <list>

	#if !defined(WINRT_BASE_H)

		#include <wrl\implements.h>

		#include <format>
		#include <string>
		#include <memory>
		#include <map>
		#include <unordered_map>
		#include <concepts>
		#include <assert.h>
	#endif

	#define TWEN_EXPORT 

#endif // !USE_MODULE

#include <d3d12.h>
#include <dxgi1_6.h>
#include <..\Vendor\DXC\inc\d3d12shader.h>

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
	//#define MODEL_VERIFY(condition) 
#else 
	#define DECLSPEC_EMPTY_BASES 
	#define MODEL_ASSERT(condtion, description) assert((condition) && description)
#endif // 

#if D3D12_MODEL_DEBUG
	#include <dxgidebug.h>

	#define DEBUG_OPERATION(...) __VA_ARGS__
#else
	#define DEBUG_OPERATION(...) 
#endif 

#if _HAS_CXX23
	#define EMPTY_IF_CXX23(...)
	#define PLACE_IF_CXX23(...) __VA_ARGS__
#else
	#define EMPTY_IF_CXX23(...) __VA_ARGS__
	#define PLACE_IF_CXX23(...)
#endif

#define TWEN_ISCA inline static constexpr auto
#define TWEN_ISC inline static constexpr
#define TWEN_IC inline constexpr
#define TWEN_SCA static constexpr auto
#define TWEN_ICA inline constexpr auto

#include "Misc\Common.h"
#include "Misc\Traits.h"