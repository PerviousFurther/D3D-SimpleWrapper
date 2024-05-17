#pragma once

namespace twen 
{
	class RenderBuffer;
	class DepthStencil;
}

namespace twen::Bindings
{
	template<typename T, bool MultiState>
	struct TransitionPart
	{
	public:
		using state_t = ::std::conditional_t<MultiState, ::std::vector<::D3D12_RESOURCE_STATES>, ::D3D12_RESOURCE_STATES>;
	public:
		inline::D3D12_RESOURCE_BARRIER Transition(
			::D3D12_RESOURCE_STATES newState,
			::UINT subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			::D3D12_RESOURCE_BARRIER_FLAGS flags = ::D3D12_RESOURCE_BARRIER_FLAG_NONE)
			requires MultiState
		{
			::D3D12_RESOURCE_STATES oldState{ m_States.front() };
			if (subresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
			{
				for (auto& state : m_States)
				{
					assert(oldState == state && "Subresource state is mismatch. TODO: resource states werent be tracked.");
					state = newState;
				}
			} else {
				assert(subresourceIndex < m_States.size() && "Out of range.");
				oldState = m_States.at(subresourceIndex);
				m_States.at(subresourceIndex) = newState;
			}
			return ResourceBarrier(static_cast<T&>(*this), oldState, newState, subresourceIndex, flags);
		}

		inline::D3D12_RESOURCE_BARRIER Transition(
			::D3D12_RESOURCE_STATES newState,
			::D3D12_RESOURCE_BARRIER_FLAGS flags = ::D3D12_RESOURCE_BARRIER_FLAG_NONE)
			requires!MultiState
		{
			auto stub = m_States;
			return ResourceBarrier(static_cast<T&>(*this), stub, m_States = newState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, flags);
		}
	protected:
		state_t m_States;
	};

	// TODO: some resource might bind an sub resource to context. But not finished.
	template<typename T> 
	struct Bindable : T
	{
#define SELECTED_BIND(...) \
{if constexpr (::std::invocable<decltype(T::CreateView), ::ID3D12Device*, const view_t*, ::D3D12_CPU_DESCRIPTOR_HANDLE>)\
					(static_cast<T*>(this)->GetDevice().operator->()->*(T::CreateView))								   \
					(&m_View, context.ObtainHandle(__VA_ARGS__));														\
				else																								   \
					(static_cast<T*>(this)->GetDevice().operator->()->*(T::CreateView))								   \
					(static_cast<T&>(*this), &m_View, context.ObtainHandle(__VA_ARGS__));}									   
	public:
		using view_t = typename T::view_t;
		using T::T;

		static constexpr auto CanBindOnRoot{ requires{ T::GraphicsBindRoot; } || requires{ T::ComputeBindRoot; } };
		static constexpr auto CanBindOnTable{ requires{ T::CreateView; } };

		template<typename ContextT>
		void Bind(ContextT& context, ::std::string_view name) 
			requires CanBindOnTable
		{
			if (context.IsOnDescriptorTable(name))
			SELECTED_BIND(name)
			else 
			{
				if constexpr (CanBindOnRoot)
					this->Bind(context, context.ObtainRoot(name));
				else assert(!"Failed to bind on pipeline.");
			}
		}

		template<typename ContextT>
		void Bind(ContextT& context, ::UINT root) 
			requires CanBindOnRoot
		{
			MODEL_ASSERT(!context.IsOnDescriptorTable(root), "Cannot replace table with root parameter.");

			if constexpr (ContextT::Type == ::D3D12_COMMAND_LIST_TYPE_DIRECT)
				(context.CommandList->*(T::GraphicsBindRoot))(root, static_cast<T*>(this)->Address(m_View));
			else
				(context.CommandList->*(T::ComputeBindRoot))(root, static_cast<T*>(this)->Address(m_View));
		}

		template<typename ContextT>
		void Bind(ContextT& context, ::UINT rootIndex, ::UINT tableIndex) 
			requires CanBindOnTable
		{
			MODEL_ASSERT(context.IsOnDescriptorTable(rootIndex), "Specified root position is not a descriptor table.");
			SELECTED_BIND(rootIndex, tableIndex);
		}
	protected:
		view_t m_View{};

#undef SELECTED_BIND
	};

}
