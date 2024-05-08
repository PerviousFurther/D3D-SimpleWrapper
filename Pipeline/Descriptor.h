#pragma once

#include "Misc\ObjectSet.h"

namespace twen 
{
	class DescriptorSet;

	template<typename T>
	class ConstantBuffer;

	class ShaderResource;
	class AccessibleBuffer;
	class SwapchainBuffer;
	class DepthStencil;
	class RenderBuffer;

	inline namespace Descriptor 
	{
		enum class Type
		{
			Unkown,

			Sampler,

			ConstantBuffer,
			ShaderResource,
			UnorderedAccessResource,

			RenderTarget,
			DepthStencil,
		};	
		struct Detail 
		{
			Descriptor::Type Type{ Descriptor::Type::Unkown };
			union // weak_ref of binding resource.
			{
				::std::weak_ptr<const AccessibleBuffer> SourceCB;
				::std::weak_ptr<const::twen::ShaderResource> SourceSR;
				struct // UAV
				{
					::std::weak_ptr<const::twen::AccessibleBuffer> SourceUA;
					::std::weak_ptr<const::twen::AccessibleBuffer> Counter;
				};
				::std::weak_ptr<const::twen::RenderBuffer> SourceRT;
				::std::weak_ptr<const::twen::DepthStencil> SourceDS;
			};
			constexpr Detail() : SourceUA{}, Counter{} {}

			Detail(::std::weak_ptr<const ShaderResource> srv)
				: Type{ Descriptor::Type::ShaderResource }
				, SourceSR{ srv } {}

			Detail(::std::weak_ptr<const AccessibleBuffer> cbv)
				: Type{ Descriptor::Type::ShaderResource }
				, SourceCB{ cbv } {}

			/*		
			Detail(::std::weak_ptr<const AccessibleBuffer> uav, ::std::weak_ptr<const AccessibleBuffer> counter = {})
				: Type{ Descriptor::Type::UnorderedAccessResource }
				, SourceUA{uav}, Counter{counter} {}
			*/

			Detail(::std::weak_ptr<const RenderBuffer> renderBuffer)
				: Type{ Descriptor::Type::RenderTarget }
				, SourceRT{renderBuffer} {}

			Detail(::std::weak_ptr<const DepthStencil> depthStencilBuffer) 
				: Type{Descriptor::Type::DepthStencil}
				, SourceDS{ depthStencilBuffer } {}

			void Discard();
			Detail& operator=(Detail const& other);
			~Detail() { Discard(); }
		};
		FORCEINLINE void Detail::Discard()
		{
			switch (Type)
			{
			case twen::Descriptor::Type::ConstantBuffer:
				SourceCB.reset();
				break;
			case twen::Descriptor::Type::ShaderResource:
				SourceSR.reset();
				break;
			case twen::Descriptor::Type::UnorderedAccessResource:
				SourceUA.reset();
				Counter.reset();
				break;			
			case twen::Descriptor::Type::RenderTarget:
				SourceRT.reset();
				break;
			case twen::Descriptor::Type::DepthStencil:
				SourceDS.reset();
				break;
			default:break;
			}
			Type =::twen::Descriptor::Type::Unkown;
		}
		FORCEINLINE Detail& Detail::operator=(const Detail& other) 
		{
			Discard();
			Type = other.Type;
			assert(Type != Descriptor::Type::Unkown&&"Detail cannot be unknown type.");
			switch (Type)
			{
			case twen::Descriptor::Type::ConstantBuffer:
				SourceCB = other.SourceCB;
				break;
			case twen::Descriptor::Type::ShaderResource:
				SourceSR = other.SourceSR;
				break;
			case twen::Descriptor::Type::UnorderedAccessResource:
				SourceUA = other.SourceUA;
				Counter = other.Counter;
				break;
			case twen::Descriptor::Type::RenderTarget:
				SourceRT = other.SourceRT;
				break;
			case twen::Descriptor::Type::DepthStencil:
				SourceDS = other.SourceDS;
				break;
			default:break;
			}
			return *this;
		}
	}

	template<>
	struct Pointer<DescriptorSet>
	{
		mutable::std::weak_ptr<DescriptorSet> Manager;

		const::UINT64 CPUHandle;
		const::UINT64 GPUHandle;
		const::UINT	  Increment;
		const::UINT	  Offset;
	
		mutable::UINT Capability;
		::UINT		  Size;

		::std::vector<Descriptor::Detail> Details;

		auto Append(::UINT count = 1u);

		void AddView(Descriptor::Detail const& newDetail);
		FORCEINLINE void Clear() { Details.clear(); Size = 0u; }

		void Discard() const;

		FORCEINLINE bool Vaild() const { return Capability; }
		// obtain current cpu handle.
		FORCEINLINE::D3D12_CPU_DESCRIPTOR_HANDLE operator*() const { return { CPUHandle + (Size - 1u) * Increment }; }
		// obtain base cpu handle.
		FORCEINLINE operator::D3D12_CPU_DESCRIPTOR_HANDLE() const { return { CPUHandle }; }
		FORCEINLINE operator::D3D12_GPU_DESCRIPTOR_HANDLE() const { return { GPUHandle }; }

		FORCEINLINE::D3D12_GPU_DESCRIPTOR_HANDLE operator()(::UINT index) const 
		{
			assert(GPUHandle && "Current gpu handle cannot be access.");
			assert(Capability && "Empty block is not allow to access.");
			assert(index < Size && "Access violation on descriptor heap.");
			return { GPUHandle + index * Increment };
		}
		FORCEINLINE::D3D12_CPU_DESCRIPTOR_HANDLE operator[](::UINT index) const 
		{ 
			assert(Capability && "Empty block is not allow to access.");
			assert(index < Size && "Access violation on descriptor heap.");
			return { CPUHandle + index * Increment };
		}
	};

	class DescriptorSet : public ShareObject<DescriptorSet>, public DeviceChild, 
		public SingleNodeObject, public SetManager<DescriptorSet>
	{
	public:
		using sort_t = ::D3D12_DESCRIPTOR_HEAP_TYPE;
		using pointer_t = Pointer<DescriptorSet>;

		FORCEINLINE static::std::unordered_set<Descriptor::Type> ResolvePermittedType(::D3D12_DESCRIPTOR_HEAP_TYPE type) 
		{
			switch (type)
			{
			case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
				return {Descriptor::Type::ConstantBuffer, Descriptor::Type::ShaderResource, Descriptor::Type::UnorderedAccessResource};
			case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
				return {Descriptor::Type::Sampler};
			case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
				return {Descriptor::Type::RenderTarget};
			case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
				return {Descriptor::Type::DepthStencil};
			case D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES:
			default:assert(!"Unknown descriptor heap type."); return{};
			}
		}
	public:
		FORCEINLINE DescriptorSet(Device& device, ::D3D12_DESCRIPTOR_HEAP_TYPE type,
			::UINT numDescriptor, ::D3D12_DESCRIPTOR_HEAP_FLAGS flags = ::D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
			: DeviceChild{ device }, SingleNodeObject{ device.NativeMask }
			, SetManager{ numDescriptor }
			, Type{ type }, Increment{ device->GetDescriptorHandleIncrementSize(type) }
			, ShaderVisible{ flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE }
			, PermittedType{ ResolvePermittedType(type) }
		{
			::D3D12_DESCRIPTOR_HEAP_DESC desc{ type, numDescriptor, flags, NativeMask };
			device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_Handle.put()));

			SET_NAME(m_Handle, ::std::format(L"DescriptorHeap{}", ID));
		}
		~DescriptorSet() = default;
	public:
		void Bind(::ID3D12GraphicsCommandList* commandList, ::std::shared_ptr<DescriptorSet> other) 
		{
			assert((Type == ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || Type ==::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
				&& "This method is use for dynamic descriptor set. But current type is not a dynamic descriptor set.");
			assert((other->Type == ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || other->Type == ::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
				&& "This method is use for dynamic descriptor set. But current type is not a dynamic descriptor set.");
			assert(other->Type != Type && "The descriptor type have been bind earlier.");
			::ID3D12DescriptorHeap* heaps[]{ m_Handle.get(), other->m_Handle.get() };
			commandList->SetDescriptorHeaps(2u, heaps);
		}
		pointer_t AddressOf(::UINT offset, ::UINT count)
		{
			assert(count <= NumsElements && "Too big.");
			return 
			{	
				weak_from_this(),
				m_Handle->GetCPUDescriptorHandleForHeapStart().ptr + offset * Increment, 
				ShaderVisible ? m_Handle->GetGPUDescriptorHandleForHeapStart().ptr + offset * Increment : 0u,
				Increment, offset, count, 0u, ::std::vector<Descriptor::Detail>{count}
			};
		}
		void DiscardAt(pointer_t const& pointer) const { pointer.Capability = 0u; }
		operator::ID3D12DescriptorHeap* () const { return m_Handle.get(); }
	public:
		::ID3D12DescriptorHeap* operator*() const { return m_Handle.get(); }
	public:
		const sort_t Type;
		const::UINT	 Increment;
		const bool	 ShaderVisible : 1;
		const::std::unordered_set<Descriptor::Type> PermittedType;
	private:
		ComPtr<::ID3D12DescriptorHeap> m_Handle;
	};

	// inline definition.

	inline auto Pointer<DescriptorSet>::Append(::UINT count)
	{
		::D3D12_CPU_DESCRIPTOR_HANDLE result{};
		if (Size + count > Capability)
			if (!Manager.lock()->Resize(*this, count - Capability + Size))
			{ assert(!"Current block cannot fill any other descriptor."); return result; }
		result.ptr = CPUHandle + Increment * Size;
		Size += count;
		return result;
	}
	FORCEINLINE void Pointer<DescriptorSet>::AddView(Descriptor::Detail const& newDetail)
	{
		assert(Manager.lock()->PermittedType.contains(newDetail.Type) && "Descriptor type mismatch with descriptor heap.");
		assert(Size < Capability && "Fill descriptor out of range.");
		Details.at(Size++) = newDetail;
	}
	FORCEINLINE void Pointer<DescriptorSet>::Discard() const 
	{
		if (Capability) 
		{
			assert(!Manager.expired()&&"Owner set was corrupted!");
			Manager.lock()->Collect(*this);
		}
	}
}

// range [begin, end)
//class DescriptorSpan
//{
//	struct Handles
//	{
//		::D3D12_CPU_DESCRIPTOR_HANDLE CPU;
//		::D3D12_GPU_DESCRIPTOR_HANDLE GPU;
//	};
//
//	struct Iterator 
//	{
//		void operator++(int) { GPU += Incremnt; CPU += Incremnt; }
//		void operator++() { GPU += Incremnt; CPU += Incremnt; }
//		Handles operator*() const { return { GPU, CPU }; }
//		bool operator==(Iterator const& other) const 
//		{ 
//			assert(CPU == other.CPU && GPU == other.GPU);
//			return CPU == other.CPU; 
//		}
//
//		::UINT64 GPU;
//		::UINT64 CPU;
//		::UINT64 Incremnt;
//	};
//
//public:
//	using iterator_t = Iterator;
//
//public:
//	DescriptorSpan(Device& device, ::ID3D12DescriptorHeap* handle, 
//		::D3D12_DESCRIPTOR_HEAP_DESC const& desc, ::UINT blockSize) 
//		: m_Handle{handle}, NumDescriptor{desc.NumDescriptors}
//		, Type{desc.Type}, Increment{blockSize} {}
//	DescriptorSpan(::std::shared_ptr<DescriptorSpan> parent, ::UINT offset) 
//		: Type{parent->Type}, Increment{parent->Increment}
//		, m_Handle{parent->m_Handle}, NumDescriptor{parent->NumDescriptor}
//		, m_BaseOffset{offset} {}
//public:
//	::ID3D12DescriptorHeap* const Get() const { return m_Handle; }
//
//	FORCEINLINE::D3D12_CPU_DESCRIPTOR_HANDLE GetHandleCPU(::UINT index) const
//	{ 
//		assert(index < NumDescriptor && "Current index overflow.");
//		return { m_Handle->GetCPUDescriptorHandleForHeapStart().ptr + m_BaseOffset + index * Increment }; 
//	}
//	FORCEINLINE::D3D12_GPU_DESCRIPTOR_HANDLE GetHandleGPU(::UINT index) const
//	{
//		assert(index < NumDescriptor && "Current index overflow.");
//		return { m_Handle->GetGPUDescriptorHandleForHeapStart().ptr + m_BaseOffset + index * Increment };
//	}
//	bool Vaild() const { return m_Manager.expired(); }
//protected:
//	::D3D12_DESCRIPTOR_HEAP_TYPE	Type;
//	::UINT							NumDescriptor;
//	::UINT							Increment;
//	::UINT							m_BaseOffset{0u};
//
//	const::UINT64 m_BaseCPUHandle;
//	const::UINT64 m_BaseGPUHandle;
//
//	::ID3D12DescriptorHeap* const m_Handle;
//
//	::std::weak_ptr<DescriptorSet> m_Manager;
//};