#pragma once

#include "Resource.h"
#include "Bindable.hpp"

namespace twen
{
	struct DirectContext;
	struct CopyContext;

	// copy convention.
	inline namespace method
	{
		inline constexpr struct FlatCopy
		{
			inline constexpr void operator()(MappedDataT dst, ::std::span<const::std::byte> src) const
			{
				::std::ranges::copy(src, dst.subspan(Offset, src.size()).begin());
			}
			inline constexpr::std::vector<::std::byte> operator()(::std::span<::std::byte> const& src) const
			{
				return src.subspan(Offset, src.size() - Offset) | ::std::ranges::to<::std::vector>();
			}

			::std::size_t Offset;
		} FlatCopyFromBase{ 0 };

		// To express image data. Use {(image).data(), width, height }.
		struct ImageCopy
		{
			using ImageSpan = ::std::mdspan<const::std::byte, ::std::dextents<::std::size_t, 2u>>;
			
		#if _HAS_CXX23
			// no responiblity for controlling source position.
			// consume the image is row major.
			inline constexpr void operator()(MappedDataT dst, ImageSpan img) const
			{
				const auto row{ img.extent(1) }, width{ img.extent(0) };
				const::std::span src{ img.data_handle(), row * width };
				operator()(dst, src, width);
			}
		#endif

			inline constexpr void operator()(MappedDataT dst, ::std::span<const::std::byte> src, ::std::uint64_t pitch) const
			{
				MODEL_ASSERT(src.size() / pitch >= Rows, "Row too small");

				for (auto i{ 0u }; i < Rows; i++)
					::std::ranges::copy
					(
						src.subspan(i * pitch, pitch), 
						dst.subspan(Offset + RowPitch * i, RowPitch).begin()
					);
			}

			// Assume the image is row major.
			inline constexpr::std::vector<::std::byte> operator()(const MappedDataT src) const
			{
				assert(Width && Rows && "Not allow zero rows or width");
				assert(Width * Rows <= src.size() && "Image data mismatch.");

				::std::vector<::std::byte> result{ Width * Rows };
				for (auto i{ 0u }; i < Rows; i++)
					::std::ranges::copy(src.subspan(Offset + i * RowPitch, Width), result.begin() + i * Width);

				return result;
			}

			::std::size_t RowPitch; // Stride of each row.
			::std::size_t Offset;	// Offset from resource base.
			::std::size_t Rows;		// Image height.
			::std::size_t Width;	// Image width.
		};
	}

	// Accessible buffer describe readback buffer or upload buffer.
	class DECLSPEC_NOVTABLE AccessibleBuffer : public ShareObject<AccessibleBuffer>, public DeviceChild
	{
	public:
		enum AccessMode { AccessRead = 3, AccessWrite = 2 };
	public:
		virtual ~AccessibleBuffer() = default;
	protected:
		//AccessibleBuffer(Pointer const& address)
		//	: DeviceChild{ address.Resource.lock()->GetDevice() }, 
		//	m_Handle{address.Resource}, m_Pointer{address} {}
		AccessibleBuffer(Device& device, ::UINT64 size, AccessMode access, 
			::UINT alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
			: DeviceChild{device}
			, Access{ access } 
			, m_Resource
			{
				Resource::Create<CommittedResource>(device,
					static_cast<::D3D12_HEAP_TYPE>(access), ResourceDesc(size, alignment),
					access == AccessWrite ? ::D3D12_RESOURCE_STATE_GENERIC_READ : ::D3D12_RESOURCE_STATE_COPY_DEST)
			} { InitMap(); }
		AccessibleBuffer(Device& device, Pointer<Heap> const& position, 
			::UINT alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
			: DeviceChild{device}
			, Access{position.Backing.lock()->Type ==::D3D12_HEAP_TYPE_READBACK ? AccessRead : AccessWrite } 
			, m_Resource
			{
				Resource::Create<PlacedResource>(device, 
					position, ResourceDesc(position.Size, alignment),
					AccessWrite ? ::D3D12_RESOURCE_STATE_GENERIC_READ : ::D3D12_RESOURCE_STATE_COPY_DEST)
			} { 
			assert(position.Backing.lock()->Type !=::D3D12_HEAP_TYPE_DEFAULT && "Defualt heap cannot be access."); 
			InitMap();
		}
	public:
		const AccessMode Access;
		// Using struct inside namespace twen::method or expressing lambda to parameter write.
		// function will be expressed (std::span<std::byte>, provided data).
		template<typename Fn, typename DT>
		inline void Write(Fn&& write, DT data)
		{
			assert(Access & AccessMode::AccessWrite && "Current buffer not allow write.");
			write(m_Mapped, data);
		}

		// Using struct inside namespace twen::method or expressing lambda to parameter read.
		// function will be expressed {std::span<std::byte>}.
		template<typename Fn>
		inline::std::vector<::std::byte> Read(Fn&& read) const
		{
			assert(Access & AccessMode::AccessRead && "Current buffer not allow read.");
			return read(m_Mapped);
		}
		inline::ID3D12Resource* operator*() const { return *m_Resource; }
	protected:
		::std::shared_ptr<Resource>	m_Resource;
		::std::span<::std::byte>	m_Mapped;
	private:
		void InitMap()
		{
			::std::byte* mapped{ nullptr };
			ID3D12Resource* resource = *m_Resource;
			resource->Map(0, nullptr, (void**)&mapped);
			assert(mapped && "Resource map failure.");

			m_Mapped =::std::span(mapped, m_Resource->Size);
		}
	};

	class Texture;

	class TextureCopy : public AccessibleBuffer
	{
		friend struct CopyContext;
	public:
		TextureCopy(::std::shared_ptr<Texture> texture, AccessMode access);
		TextureCopy(Device& device, AccessMode access);
	public:
		template<typename ContextT>
		void Copy(ContextT& copyContext);
		template<typename ContextT>
		void Copy(ContextT& copy, ::std::shared_ptr<Texture> other, ::UINT subresourceIndex);
#if _HAS_CXX23
		// Copy single image to sub resource.
		void Write(ImageCopy::ImageSpan img, ::UINT subresourceIndex = 0u);
#endif
		// Copy single image to sub resource.
		void Write(::std::span<const::std::byte> img, ::UINT rowPitchOfSrc, ::UINT subresourceIndex = 0u);
	private:
		::std::weak_ptr<Texture> m_Target;
	};

	class DECLSPEC_EMPTY_BASES ReadbackBuffer : public AccessibleBuffer
	{
	public:
		ReadbackBuffer(Device& device, ::UINT64 size) 
			: AccessibleBuffer{ device, size, AccessMode::AccessRead } {}

		auto Write(auto...) = delete;
	};
	
	// Vertex buffer.
	class VertexBuffer : AccessibleBuffer
	{
	public:
		template<::std::ranges::range...Rngs>
		VertexBuffer(Device& device, Rngs&&...ranges) :
			AccessibleBuffer{ device,
				((::std::ranges::size(ranges) * sizeof(::std::ranges::range_value_t<Rngs>)) + ...),
				AccessWrite
		} {
			auto base = m_Resource->BaseAddress();
			auto accumlate = 0ull;
			((
			// begin fold expression.
				m_Views.emplace_back(base + accumlate,
					static_cast<::UINT>(::std::ranges::size(ranges) * sizeof(::std::ranges::range_value_t<Rngs>)),
					static_cast<::UINT>(sizeof(::std::ranges::range_value_t<Rngs>))),
				m_VertexCounts.emplace_back(static_cast<::UINT>(::std::ranges::size(ranges))),
				// upload to resource.
				Write(FlatCopy{ accumlate }, ::std::span{ ::std::bit_cast<const::std::byte*>(::std::ranges::data(ranges)), m_Views.back().SizeInBytes }),
				// offset.
				accumlate += m_Views.back().SizeInBytes),
			// end fold expression.
			...);
		}
	public:
		void Bind(DirectContext& context, ::UINT startSlot = 0u, ::UINT dropCount = 0u) const;

		::UINT SlotCount() const { return static_cast<::UINT>(m_VertexCounts.size()); }

		::UINT VertexCount(::UINT index) const { return m_VertexCounts.at(index); }
		::std::vector<::UINT> const& VertexCount() const { return m_VertexCounts; }

		template<::std::ranges::range...Rngs>
		void Reupload(Rngs&&...);
	private:
		::std::vector<::UINT> m_VertexCounts;
		::std::vector<::D3D12_VERTEX_BUFFER_VIEW> m_Views;
	};

	class IndexBuffer : AccessibleBuffer
	{
	private:
		template<::std::integral Int>
		struct IntergalMap
		{
			static_assert(::std::_Always_false<Int>, "Intergal too large.");
		};
	public:
		template<::std::integral IntT, ::std::size_t size>
		IndexBuffer(Device& device, ::std::span<IntT, size> indices)
			: AccessibleBuffer{ device, sizeof(IntT) * indices.size(), AccessWrite }
			, m_View{ m_Resource->BaseAddress(), static_cast<::UINT>(sizeof(IntT) * indices.size()), IntergalMap<::std::remove_cv_t<IntT>>::value }
			, NumIndices{ static_cast<::UINT>(indices.size()) }
		{ Write(FlatCopyFromBase, ::std::span{ ::std::bit_cast<const::std::byte*>(indices.data()), indices.size() * sizeof(IntT) }); }

		template<::std::integral IntT, ::std::size_t size>
		void Reupload(::std::span<IntT, size>);

		//inline  void Commit(::ID3D12GraphicsCommandList* cmdlist) const
		//{ cmdlist->IASetIndexBuffer(&m_View); }

		void Bind(DirectContext& context) const;
	private:
		::UINT NumIndices;
		::D3D12_INDEX_BUFFER_VIEW m_View;
	};

	template<>
	struct IndexBuffer::IntergalMap<::UINT>
	{
		static constexpr auto value = ::DXGI_FORMAT_R32_UINT;
	};
	template<>
	struct IndexBuffer::IntergalMap<::UINT16>
	{
		static constexpr auto value = ::DXGI_FORMAT_R16_UINT;
	};
	template<>
	struct IndexBuffer::IntergalMap<::UINT8>
	{
		static constexpr auto value = ::DXGI_FORMAT_R8_UINT;
	};

	namespace ConstantBuffers
	{
		static constexpr auto MinimumAlignment{ D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT };
	}

	struct DECLSPEC_EMPTY_BASES ConstantBufferView : protected AccessibleBuffer
	{
	public:
		using view_t = ::D3D12_CONSTANT_BUFFER_VIEW_DESC;

		using AccessibleBuffer::AccessibleBuffer;

		static constexpr auto CreateView{ &::ID3D12Device::CreateConstantBufferView };
		static constexpr auto GraphicsBindRoot{ &::ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView };
		static constexpr auto ComputeBindRoot{ &::ID3D12GraphicsCommandList::SetComputeRootConstantBufferView };

		::D3D12_GPU_VIRTUAL_ADDRESS Address(::D3D12_CONSTANT_BUFFER_VIEW_DESC const& desc) const { return desc.BufferLocation; } // reserved.
	};

	// Constant buffer is not a typical accessible buffer, so derived in private.
	template<typename T>
	class ConstantBuffer;

	template<>
	class ConstantBuffer<void> :
		public Bindings::Bindable<ConstantBufferView>
	{
	public:
		// Make buffer and reserve for future usage.
		// @param alignment: constant buffer's alignment
		ConstantBuffer(Device& device, ::UINT64 reservedSize, ::UINT alignment = ConstantBuffers::MinimumAlignment)
			: Bindable{device, reservedSize, AccessWrite}
			, Alignment{alignment}
			, Capability{reservedSize}
		{ m_View = { m_Resource->BaseAddress(), static_cast<::UINT>(Capability) }; }
		// Make whole range the element.
		// @param alignment: constant buffer's alignment
		ConstantBuffer(Device& device, Pointer<Heap> const& position, ::UINT alignment)
			: Bindable{device, position, alignment}
			, Alignment{alignment}
			, Capability{position.Size}
		{ m_View = { m_Resource->BaseAddress(), static_cast<::UINT>(Capability) }; }
	public:
		template<typename T>
		inline void Update(T&& element) 
		{
			using type =::std::remove_reference_t<T>;

			assert(sizeof(type) <= Alignment && "Alignment too small.");
			assert((m_Filled + Alignment <= Capability ) && "Out of range.");

			new (m_Mapped.data() + m_Filled) type (::std::forward<type>(element));
			m_Filled += Alignment;
		}
		template<typename T>
		inline void Update(T&& element, ::UINT index) 
		{
			using type = ::std::remove_reference_t<T>;
			assert(sizeof(type) <= Alignment && "Alignment mismatch.");

			const auto offset = index * Alignment;
			assert(offset + sizeof(type) <= Capability && "Writing constant buffer out of range.");

			new (m_Mapped.data() + offset) type (::std::forward<type>(element));
		}

		void StoreOffset(::UINT64 offset) { m_View.BufferLocation += offset; }

		void Copy(DirectContext& context, ::std::shared_ptr<ConstantBuffer> other);
	public:
		const::UINT Alignment;
		const::UINT64 Capability;
	protected:
		::UINT64 m_Filled{0ull};
	};

	// This only provide some helper function.
	template<typename T>
		requires::std::is_trivial_v<T>
	class ConstantBuffer<T> : public ConstantBuffer<void> 
	{
	public:
		using base_t = ConstantBuffer<void>;
		static_assert(sizeof(T) >= ConstantBuffers::MinimumAlignment, "Size too small. Add padding to make it suitable or use keyword Alignas.");

		template<::std::convertible_to<T> PT>
		ConstantBuffer(Device& device, PT&& element)
			: ConstantBuffer<void>
			{
				device, 
				sizeof(T),
				sizeof(T) > ConstantBuffers::MinimumAlignment 
					? sizeof(T) 
					: ConstantBuffers::MinimumAlignment 
			}
		{ this->Update(static_cast<T>(::std::forward<PT>(element))); }

		template<::std::convertible_to<T> PT>
		ConstantBuffer(Device& device, Pointer<Heap> const& position, PT&& element)
			: ConstantBuffer<void>
			{
				device, sizeof(T),
				sizeof(T) > ConstantBuffers::MinimumAlignment 
					? sizeof(T) 
					: ConstantBuffers::MinimumAlignment 
			} 
		{
			assert((position.Size % sizeof(T) == 0) && "Size is not aligned or Size too small.");
			this->Update(static_cast<T>(::std::forward<PT>(element)));
		}

		template<::std::convertible_to<T> PT>
		inline void Update(PT&& element)
		{ base_t::Update(static_cast<T>(::std::forward<PT>(element))); }

		template<::std::convertible_to<T> PT>
		inline  void Update(PT&& element, ::UINT index)
		{ base_t::Update(static_cast<T>(::std::forward<PT>(element)), index); }

	private:
		using base_t::ConstantBuffer;
		using base_t::Update;
	};

}