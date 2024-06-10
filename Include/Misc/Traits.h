#pragma once

namespace twen::inner
{
	//----------------------------------------------------\
	// Offer template that can help context to bind on 
	// different view resource.
	//=====================================================\
	// INFO:
	// 
	// - Offer member: Root       
	// : indicate command list how to bind on root parameter.
	// - Offer member: CreateView 
	// : indicate command list how to create view on descriptor heap.
	// 
	//=====================================================/
	template<typename View, ::D3D12_COMMAND_LIST_TYPE type>
	struct Operation;

	//----------------------------------------------------\
	// template help context to transit resource state.
	//=====================================================\
	// INFO:
	// 
	// - Offer member : State
	// : indicate resource can have only state.
	// | Offer member : { BeforeState, AfterState }
	// : indicate resource states should transit before 
	// : used and after used.
	// - Offer member : CopyState
	// : indicate resource state during copy operation.
	//=====================================================/
	// what is more : Currently, we always transition the resource
	//                to idle state when finish using it...
	template<typename T>
	struct Transition;

	//----------------------------------------------------\
	// template help device to create specified resource with 
	// current flags.
	//=====================================================\
	// INFO:
	// 
	// - Offer member : HeapFlags
	// : indicate must require flags to create heap.
	// - Offer member : ResourceFlags
	// : indicate must require flags to create resource.
	// - Offer member : ClearValue
	// : indicate optimize value.
	// 
	//=====================================================/
	template<typename View>
	struct ResourceCreationInfo
	{
		TWEN_ISCA HeapFlags     { ::D3D12_HEAP_FLAG_NONE };
		TWEN_ISCA ResourceFlags { ::D3D12_RESOURCE_FLAG_NONE };
	};
	using ResourceNoExtra = ResourceCreationInfo<void>;

	//----------------------------------------------------\
	// Bind allocation info with view.
	template<typename View>
	struct Allocation;

	template<typename>
	struct function_traits;

	template<typename Return, typename...Args>
	struct function_traits<Return(*)(Args...)>
	{
		using type = Return(*)(Args...);
		using return_type = Return;
	};
#pragma region member function traits
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...)> : function_traits<Return(*)(Args...)>
	{
		using interface_type = Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...) const> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...) const volatile> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...) const&> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...) const&&> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...)&&> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...)&> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...)volatile&> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...)volatile&&> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...)const volatile&> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
	template<typename Return, typename Interface, typename...Args>
	struct function_traits<Return(Interface::*)(Args...)const volatile&&> : function_traits<Return(*)(Args...)>
	{
		using interface_type = const Interface;
	};
#pragma endregion 

}