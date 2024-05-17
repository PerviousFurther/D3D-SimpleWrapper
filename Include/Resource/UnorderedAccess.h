#pragma once

#include "Bindable.hpp"

namespace twen
{
	struct DirectContext;
	struct ComputeContext;

	class UnorderedAccess : public ShareObject<UnorderedAccess>
	{
	public:
		using sort_t = ::D3D12_UAV_DIMENSION;

		void Bind(DirectContext& context, ::UINT index);
		void Bind(ComputeContext& context, ::UINT index);

		const sort_t Type;
	protected:
		UnorderedAccess(Device& device, sort_t type);
	protected:
		::D3D12_UNORDERED_ACCESS_VIEW_DESC m_Desc;
	};
}