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

	namespace Descriptors 
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

		void Discard() const;

		inline bool Vaild() const { return Capability; }
		// obtain current cpu handle.
		// obtain base cpu handle.
		inline operator::D3D12_CPU_DESCRIPTOR_HANDLE() const { return { CPUHandle }; }
		inline operator::D3D12_GPU_DESCRIPTOR_HANDLE() const { return { GPUHandle }; }

		inline::D3D12_GPU_DESCRIPTOR_HANDLE operator()(::UINT index) const 
		{
			assert(GPUHandle && "Current gpu handle cannot be access.");
			assert(Capability && "Empty block is not allow to access.");
			assert(index < Capability && "Access violation on descriptor heap.");
			return { GPUHandle + index * Increment };
		}
		inline::D3D12_CPU_DESCRIPTOR_HANDLE operator[](::UINT index) const 
		{ 
			assert(Capability && "Empty block is not allow to access.");
			assert(index < Capability && "Access violation on descriptor heap.");

			return { CPUHandle + index * Increment };
		}
	};

	class DescriptorSet : public ShareObject<DescriptorSet>, public DeviceChild, 
		public SingleNodeObject, public SetManager<DescriptorSet>
	{
	public:
		using sort_t = ::D3D12_DESCRIPTOR_HEAP_TYPE;
		using pointer_t = Pointer<DescriptorSet>;
	public:
		inline DescriptorSet(Device& device, ::D3D12_DESCRIPTOR_HEAP_TYPE type,
			::UINT numDescriptor, ::D3D12_DESCRIPTOR_HEAP_FLAGS flags = ::D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
			: DeviceChild{ device }, SingleNodeObject{ device.NativeMask }
			, SetManager{ numDescriptor }
			, Type{ type }, Increment{ device->GetDescriptorHandleIncrementSize(type) }
			, ShaderVisible{ flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE }
		{
			::D3D12_DESCRIPTOR_HEAP_DESC desc{ type, numDescriptor, flags, NativeMask };
			device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_Handle.Put()));

			SET_NAME(m_Handle, ::std::format(L"DescriptorHeap{}", ID));
		}
		~DescriptorSet() = default;
	public:
		//void Bind(::ID3D12GraphicsCommandList* commandList, ::std::shared_ptr<DescriptorSet> other) 
		//{
		//	assert((Type == ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || Type ==::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		//		&& "This method is use for dynamic descriptor set. But current type is not a dynamic descriptor set.");
		//	assert((other->Type == ::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || other->Type == ::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		//		&& "This method is use for dynamic descriptor set. But current type is not a dynamic descriptor set.");
		//	assert(other->Type != Type && "The descriptor type have been bind earlier.");
		//	::ID3D12DescriptorHeap* heaps[]{ m_Handle.Get(), other->m_Handle.Get() };
		//	commandList->SetDescriptorHeaps(2u, heaps);
		//}
		pointer_t AddressOf(::UINT offset, ::UINT count)
		{
			assert(count <= NumsElements && "Too big.");
			return 
			{	
				weak_from_this(),
				//::std::vector<Descriptors::Detail>{count},
				m_Handle->GetCPUDescriptorHandleForHeapStart().ptr + offset * Increment, 
				ShaderVisible ? m_Handle->GetGPUDescriptorHandleForHeapStart().ptr + offset * Increment : 0u,
				Increment, offset, count,
			};
		}
		void DiscardAt(pointer_t const& pointer) const { pointer.Capability = 0u; }
		operator::ID3D12DescriptorHeap* () const { return m_Handle.Get(); }
	public:
		::ID3D12DescriptorHeap* operator*() const { return m_Handle.Get(); }
	public:
		const sort_t Type;
		const::UINT	 Increment;
		const::UINT	 ShaderVisible : 1;
		const::std::unordered_set<Descriptors::Type> PermittedType;
	private:
		ComPtr<::ID3D12DescriptorHeap> m_Handle;
	};

	inline void Pointer<DescriptorSet>::Discard() const 
	{
		if (Capability) 
		{
			assert(!Manager.expired()&&"Owner set was corrupted!");
			Manager.lock()->Collect(*this);
		}
	}
}

//struct Detail 
		//{
		//	Descriptors::Type Type{ Descriptors::Type::Unkown };
		//	union // weak_ref of binding resource.
		//	{
		//		::std::weak_ptr<const AccessibleBuffer> SourceCB;
		//		::std::weak_ptr<const::twen::ShaderResource> SourceSR;
		//		struct // UAV
		//		{
		//			::std::weak_ptr<const::twen::AccessibleBuffer> SourceUA;
		//			::std::weak_ptr<const::twen::AccessibleBuffer> Counter;
		//		};
		//		::std::weak_ptr<const::twen::RenderBuffer> SourceRT;
		//		::std::weak_ptr<const::twen::DepthStencil> SourceDS;
		//	};
		//	constexpr Detail() : SourceUA{}, Counter{} {}

		//	Detail(::std::weak_ptr<const ShaderResource> srv)
		//		: Type{ Descriptors::Type::ShaderResource }
		//		, SourceSR{ srv } {}

		//	Detail(::std::weak_ptr<const AccessibleBuffer> cbv)
		//		: Type{ Descriptors::Type::ShaderResource }
		//		, SourceCB{ cbv } {}

		//	/*		
		//	Detail(::std::weak_ptr<const AccessibleBuffer> uav, ::std::weak_ptr<const AccessibleBuffer> counter = {})
		//		: Type{ Descriptors::Type::UnorderedAccessResource }
		//		, SourceUA{uav}, Counter{counter} {}
		//	*/

		//	Detail(::std::weak_ptr<const RenderBuffer> renderBuffer)
		//		: Type{ Descriptors::Type::RenderTarget }
		//		, SourceRT{renderBuffer} {}

		//	Detail(::std::weak_ptr<const DepthStencil> depthStencilBuffer) 
		//		: Type{Descriptors::Type::DepthStencil}
		//		, SourceDS{ depthStencilBuffer } {}

		//	void Discard();
		//	Detail& operator=(Detail const& other);
		//	~Detail() { Discard(); }
		//};
		//inline  void Detail::Discard()
		//{
		//	switch (Type)
		//	{
		//	case Descriptors::Type::ConstantBuffer:
		//		SourceCB.reset();
		//		break;
		//	case Descriptors::Type::ShaderResource:
		//		SourceSR.reset();
		//		break;
		//	case Descriptors::Type::UnorderedAccessResource:
		//		SourceUA.reset();
		//		Counter.reset();
		//		break;			
		//	case Descriptors::Type::RenderTarget:
		//		SourceRT.reset();
		//		break;
		//	case Descriptors::Type::DepthStencil:
		//		SourceDS.reset();
		//		break;
		//	default:break;
		//	}
		//	Type = Descriptors::Type::Unkown;
		//}
		//inline Detail& Detail::operator=(const Detail& other) 
		//{
		//	Discard();
		//	Type = other.Type;
		//	assert(Type != Descriptors::Type::Unkown&&"Detail cannot be unknown type.");
		//	switch (Type)
		//	{
		//	case Descriptors::Type::ConstantBuffer:
		//		SourceCB = other.SourceCB;
		//		break;
		//	case Descriptors::Type::ShaderResource:
		//		SourceSR = other.SourceSR;
		//		break;
		//	case Descriptors::Type::UnorderedAccessResource:
		//		SourceUA = other.SourceUA;
		//		Counter = other.Counter;
		//		break;
		//	case Descriptors::Type::RenderTarget:
		//		SourceRT = other.SourceRT;
		//		break;
		//	case Descriptors::Type::DepthStencil:
		//		SourceDS = other.SourceDS;
		//		break;
		//	default:break;
		//	}
		//	return *this;
		//}
