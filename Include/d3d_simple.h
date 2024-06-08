//*****
//* Export page.
//* blah blah...
#pragma once

#include "mpch.h"

#include "System\Adapter.h"

#include "Misc\Traits.h"

#include "System\Device.h"
#include "System\Residency.h"

#include "Resource\Resource.h"
#include "Resource\ResourceAllocator.hpp"
#include "Resource\Bindable.hpp"

#include "Pipeline\Default.h"
#include "Pipeline\Descriptor.h"
#include "Pipeline\Compiler.h"
#include "Pipeline\Pipeline.h"

#include "Command\Query.h"
#include "Command\Indirect.hpp"
#include "Command\Context.h"
//#include "Command\Commands.h"

#ifndef DISABLE_CAMERA 
	#include "Helper\Camera.hpp"
#endif

#ifndef DIABLE_MESH_GENERATOR
	#include "Helper\MeshGenerator.hpp"
#endif
namespace twen::inner
{
	inline ::ID3D12Device* DeviceChild::GetDevicePointer() const { return *m_Device; }
	inline bool DeviceChild::IsSameAdapter(DeviceChild const& /*other*/) const
	{
		return false; // temporary.
	}
}

namespace twen 
{
	// using extension will make resource order become correct.
	template<typename View>
	auto Device::Create(ResourceOrder&& order, ::std::string_view name) requires
		requires{ typename View::view; }
	{
		using namespace inner;

		using view = typename View::view;
		using extension = typename View::extension;

		order.Flags |= ResourceCreationInfo<view>::ResourceFlags;
		//BUG: Weird because |= cannot find.
		order.HeapFlags = order.HeapFlags | ResourceCreationInfo<view>::HeapFlags;
		order.HeapType = ResourceCreationInfo<view>::HeapType;

		if constexpr (requires{ extension::Revise(order); })
			extension::Revise(order);

		// /- TODO: temporary...should not immediately allocate resources. -\

		inner::Pointer<Resource> pointer;
		if constexpr (requires{ Transition<view>{}; })
			pointer = m_Allocator->Allocate(this, order, Transition<view>{}(ResourceCurrentStage::Idle));
		else
			pointer = m_Allocator->Allocate(this, order, ::D3D12_RESOURCE_STATE_COMMON);

		::std::shared_ptr<View> result;
		if constexpr (::std::constructible_from<View, Device&, ResourceOrder const&, ::std::string_view>)
			result = ::std::make_shared<View>(*this, order, name);
		else 
			result = ::std::make_shared<View>(*this, name);

		result->Pointer(::std::move(pointer));
		// \- END TODO                                                     -/
		return result;
	}
}

#undef EMPTY_IF_CXX23
#undef PLACE_IF_CXX23
#undef TWEN_ISCA
#undef TWEN_ISC
#undef TWEN_SCA
#undef TWEN_ICA
#undef TWEN_IC
#undef D3D12_MODEL_DEBUG
#undef DEBUG_OPERATION
#undef MODEL_ASSERT