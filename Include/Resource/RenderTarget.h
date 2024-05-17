#pragma once

#include "Resource.h"
#include "Bindable.hpp"

namespace twen::RenderTargets
{
	inline constexpr struct {
		inline constexpr bool operator()(::D3D12_RESOURCE_DIMENSION res, ::D3D12_RTV_DIMENSION rtv) const noexcept
		{
			switch (res)
			{
			case D3D12_RESOURCE_DIMENSION_BUFFER:
				return false;
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				return false;
			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				switch (rtv)
				{
				case::D3D12_RTV_DIMENSION_TEXTURE2D:
				case::D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
				case::D3D12_RTV_DIMENSION_TEXTURE2DMS:
				case::D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
					return true;
				default:return false;
				}
			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				switch (rtv)
				{
				case D3D12_RTV_DIMENSION_TEXTURE3D:
					return true;
				default:return false;
				}
			default:return false;
			}
		}

		inline constexpr bool operator()(::D3D12_RESOURCE_DIMENSION res, ::D3D12_DSV_DIMENSION dsv) const noexcept
		{
			switch (res)
			{
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				return false;

			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				switch (dsv)
				{
				case::D3D12_DSV_DIMENSION_TEXTURE2D:
				case::D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
				case::D3D12_DSV_DIMENSION_TEXTURE2DMS:
				case::D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
					return true;
				default:return false;
				}
			default:return false;
			}
		}
	} IsDimensionMatch;
}

namespace twen 
{
	struct Context;
	struct DirectContext;
	struct CopyContext;
	struct ComputeContext;
	class TextureCopy;
	class DepthStencil;

	class SwapchainBuffer : public ShareObject<SwapchainBuffer>
	{
		friend class RenderBuffer;
	public:
		SwapchainBuffer(ComPtr<::IDXGISwapChain4> swapchain)
			: m_Swapchain{swapchain}
		{
			::DXGI_SWAP_CHAIN_DESC1 desc;
			swapchain->GetDesc1(&desc);

			m_RenderBuffer.resize(desc.BufferCount);
			for (auto i{ 0u }; auto & buffer : m_RenderBuffer) 
			{
				swapchain->GetBuffer(i++, IID_PPV_ARGS(buffer.Put()));
				assert(buffer && "Get buffer failure.");
			}
			m_Desc = m_RenderBuffer.back()->GetDesc();
			m_States.resize(desc.BufferCount);
		}

		::D3D12_RESOURCE_BARRIER Transition(::D3D12_RESOURCE_STATES newState, 
			::D3D12_RESOURCE_BARRIER_FLAGS flags =::D3D12_RESOURCE_BARRIER_FLAG_NONE)
		{
			auto index{ m_Swapchain->GetCurrentBackBufferIndex() };
			auto oldState{ m_States.at(index) };
			return ResourceBarrier(m_RenderBuffer[index].Get(), oldState, m_States.at(index) = newState, flags);
		}

		::D3D12_RESOURCE_DESC Desc() const { return m_Desc; }
		operator::ID3D12Resource* () const { return m_RenderBuffer.at(m_Swapchain->GetCurrentBackBufferIndex()).Get(); }
	private:
		::D3D12_RESOURCE_DESC m_Desc;
		::UINT m_Index;
		::std::vector<ComPtr<::ID3D12Resource>> m_RenderBuffer;
		::std::vector<::D3D12_RESOURCE_STATES> m_States;
		ComPtr<::IDXGISwapChain4> m_Swapchain;
	};

	class RenderBuffer : public ShareObject<RenderBuffer>, public DeviceChild
		, public Bindings::TransitionPart<RenderBuffer, true>
	{
	public:
		static constexpr::DXGI_FORMAT DefaultFormat{ ::DXGI_FORMAT_R8G8B8A8_UNORM };
		static constexpr::D3D12_CLEAR_VALUE DefaultClearValue{ DefaultFormat, 0.5f, 0.5f, 0.5f, 1.0f };

		struct ViewDesc
		{
			::UINT MipSlice;
			::UINT PlaneSlice;
			::UINT FirstArraySlice;
			::UINT ArraySize;
		};
	public:
		RenderBuffer(Device& device, ::D3D12_RESOURCE_DESC const& desc, ::D3D12_RTV_DIMENSION dimension,
			::D3D12_CLEAR_VALUE const& clearValue = DefaultClearValue, ::UINT visbleNode = 0u)
			: DeviceChild{ device }
			, Desc{ desc }
			, m_Resource{ device.Create<CommittedResource>(device, ::D3D12_HEAP_TYPE_DEFAULT, desc,
				::D3D12_RESOURCE_STATE_RENDER_TARGET, visbleNode, ::D3D12_HEAP_FLAG_NONE, &clearValue) }
			, m_View{ desc.Format, dimension } {
			assert(RenderTargets::IsDimensionMatch(desc.Dimension, dimension) && "Render target dimension mismatch. Btw...Buffer/Tex1D rtv is not supported.");
			assert(
				desc.SampleDesc.Count > 1u ?
				desc.DepthOrArraySize > 1u ?
				dimension == ::D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY
				: dimension == D3D12_RTV_DIMENSION_TEXTURE2DMS
				: true && "Render target view dimension mismatched.");

			m_States.resize(desc.DepthOrArraySize * desc.MipLevels, ::D3D12_RESOURCE_STATE_RENDER_TARGET);
			SetView({ 0u, 0u, 0u, desc.DepthOrArraySize });
		}

		void Bind(DirectContext& context, ::UINT index) const;
		void Copy(DirectContext& context, ::std::shared_ptr<SwapchainBuffer> destination, ::UINT subresourceIndex = 0u);

		void Copy(CopyContext& UNFINISHED, ::std::shared_ptr<TextureCopy>);

		operator::ID3D12Resource* () const { return *m_Resource; }
	public:
		const::D3D12_RESOURCE_DESC Desc;
	private:
		void SetView(ViewDesc const& viewDesc);
	private:
		::D3D12_RENDER_TARGET_VIEW_DESC m_View;
		::std::shared_ptr<Resource> m_Resource;
	};
	inline void RenderBuffer::SetView(ViewDesc const& viewDesc)
	{
		switch (m_View.ViewDimension)
		{
		case D3D12_RTV_DIMENSION_TEXTURE2D:
			m_View.Texture2D = { viewDesc.MipSlice, viewDesc.PlaneSlice, };
			break;
		case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: // reserved.
			m_View.Texture2DArray = { viewDesc.MipSlice, viewDesc.FirstArraySlice, viewDesc.ArraySize, viewDesc.PlaneSlice };
			break;
		case D3D12_RTV_DIMENSION_TEXTURE2DMS:break;
		case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
			m_View.Texture2DMSArray = { viewDesc.FirstArraySlice, viewDesc.ArraySize };
			break;
		case D3D12_RTV_DIMENSION_TEXTURE3D:
			m_View.Texture3D = { viewDesc.MipSlice, viewDesc.FirstArraySlice, viewDesc.ArraySize };
			break;
		default:assert(!"Not support."); break;
		}
	}

	class DepthStencil : public ShareObject<DepthStencil>, public DeviceChild
		, Bindings::TransitionPart<DepthStencil, false>
	{
	public:
		static constexpr auto DefaultFormat{ ::DXGI_FORMAT_D32_FLOAT };
		static constexpr::D3D12_DEPTH_STENCIL_VALUE DefaultClearValue{ 1.0f, 0xff };

		struct ViewDesc
		{
			::UINT MipSlice;
			::UINT FirstArraySlice;
			::UINT ArraySize;
		};
	public:
		DepthStencil(Device& device, ::D3D12_RESOURCE_DESC const& desc, ::D3D12_DSV_DIMENSION dimension,
			::UINT visible = 0u, ::D3D12_CLEAR_VALUE const& clrVal = { .Format{DefaultFormat}, .DepthStencil{DefaultClearValue} })
			: DeviceChild{ device }
			, Desc{ desc }
			, m_Resource
			{
				device.Create<CommittedResource>(::D3D12_HEAP_TYPE_DEFAULT, desc,
					::D3D12_RESOURCE_STATE_DEPTH_WRITE, visible,
					::D3D12_HEAP_FLAG_NONE, &clrVal)
			}
		{
			assert(RenderTargets::IsDimensionMatch(desc.Dimension, dimension) && "Depth stencil mismatch.");
			assert(::DXGI_FORMAT_UNKNOWN != Utils.GetDSVFormat(desc.Format) && "Wrong depth stencil format.");
			assert(
				desc.SampleDesc.Count > 1u ?
				desc.DepthOrArraySize > 1u ?
				dimension == ::D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY
				: dimension == D3D12_DSV_DIMENSION_TEXTURE2DMS
				: true && "Render target view dimension mismatched.");

			m_View.Format = desc.Format;
			m_View.ViewDimension = dimension;

			SetView({ 0u, 0u, desc.DepthOrArraySize });
		}

		void Flags(::D3D12_DSV_FLAGS flags) { m_View.Flags = flags; }

		void Copy(DirectContext& context, auto UNFINISHED, ::UINT subresourceIndex = 0u);
		void Bind(DirectContext& context) const;
	public:
		const::D3D12_RESOURCE_DESC Desc;
	private:
		void SetView(ViewDesc const& desc);
	private:
		::D3D12_DEPTH_STENCIL_VIEW_DESC m_View{};

		::std::shared_ptr<Resource> m_Resource;
	};
	FORCEINLINE void DepthStencil::SetView(ViewDesc const& desc)
	{
		switch (m_View.ViewDimension)
		{
		case D3D12_DSV_DIMENSION_TEXTURE1D:
		case D3D12_DSV_DIMENSION_TEXTURE2D:
			m_View.Texture2D = { desc.MipSlice };
			break;
		case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
		case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
			m_View.Texture2DArray = { desc.MipSlice, desc.FirstArraySlice, desc.ArraySize };
			break;
		case D3D12_DSV_DIMENSION_TEXTURE2DMS:break;
		case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
			m_View.Texture2DMSArray = { desc.FirstArraySlice, desc.ArraySize };
			break;
		default:assert(!"Unsupported format."); break;
		}
	}
}