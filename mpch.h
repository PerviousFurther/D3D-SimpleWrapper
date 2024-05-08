#pragma once

#include <set>
#include <unordered_set>

#include <mdspan>
#include <ranges>

#include <cassert>

#include <vendor\base_4.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#ifdef _MSC_VER
	#define DECLSPEC_EMPTY_BASES __declspec(empty_bases)
#else 
	#define DECLSPEC_EMPTY_BASES 
#endif // 

#if defined(_DEBUG) || defined(ENABLE_D3D12_MODEL_DEBUG)
	#define D3D12_MODEL_DEBUG 1
#else
	#define D3D12_MODEL_DEBUG 0
#endif

#if D3D12_MODEL_DEBUG
	#include <dxgidebug.h>

	#define DEBUG_OPERATION(x) x
	#include "Misc\Debug.h"
#else
	#define DEBUG_OPERATION(x) 
#endif 

#include "Misc\Common.h"