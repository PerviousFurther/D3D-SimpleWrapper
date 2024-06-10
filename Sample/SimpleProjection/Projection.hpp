
#include "d3d_simple.h"

using namespace twen;

struct Sample_SkyBox_Projection
{
	Sample_SkyBox_Projection(::IDXGISwapChain4* swapchain)
	{
		Create_RTDS(swapchain);
		Create_Res();
		Create_Pipeline();
	}

	~Sample_SkyBox_Projection() = default;

	struct BufferUploader
	{
		template<typename T>
		BufferUploader(T&& w)
			: Position{ ::std::move(w.Pos) }
			, UVs{ ::std::move(w.UV) }
		{}

		auto operator()(::std::span<::std::byte> data)
		{
			WriteResult<VertexBuffer> result{};

			::std::span tdata{ ::std::bit_cast<::std::byte*>(Position.data()), sizeof(Meshes::Vec3) * Position.size() };
			::std::ranges::copy(tdata, data.data());

			result.emplace_back(sizeof(Meshes::Vec3), tdata.size());

			auto next = data.begin() + tdata.size();

			tdata = { ::std::bit_cast<::std::byte*>(UVs.data()), sizeof(Meshes::Vec2) * UVs.size() };
			::std::ranges::copy(tdata, next);

			result.emplace_back(sizeof(Meshes::Vec2), tdata.size());

			return result;
		}

		::std::vector<Meshes::Vec3> Position;
		::std::vector<Meshes::Vec2> UVs;
	};

	struct TextureWriter
	{
		TextureWriter()
		{
			for (auto x{ 0u }; x < 128; ++x)
				for (auto y{ 0u }; y < 128; ++y)
				{
					auto factor = (x * 2) & 0xff;
					bytes[y * 128 + x] = (0xff << 24u) | (factor << 8) | (factor << 16) | (factor);
				}
		}

		WriteResult<StagingBuffer> operator()(::std::span<::std::byte> b)
		{
			auto const& footprints = target->BackingResource()->Footprints();
			auto const& footprint = footprints.front();
			::std::span this_data{ ::std::bit_cast<::std::byte*>(&this->bytes[0]), 128 * 128 * 4u };

			for (auto i{ 0u }; i < footprint.Footprint.Height; i++)
				::std::ranges::copy
				(
					this_data.subspan(i * 128u * 4u, 128u * 4u),
					b.subspan(footprint.Footprint.RowPitch * i + footprint.Offset, footprint.Footprint.RowPitch).begin()
				);
		}

		Views::Resource* target;
		unsigned int bytes[128 * 128]{};
	};

	void Create_Res()
	{
		auto staging = device.Create<StagingBuffer>(Utils::DescSize(ResourceOrder{ 128, 128, ::DXGI_FORMAT_R8G8B8A8_UNORM }), "Staging");

		TextureWriter w{};
		w.target = t.get();
		staging->Write(w);

		device.GetCommandContext()->ExecuteOnBatch(
			[=](DirectCommandContext* copy)
			{
				copy->Transition(sky, ::D3D12_RESOURCE_STATE_COPY_DEST);
				copy->Transition(t,   ::D3D12_RESOURCE_STATE_COPY_DEST);
				copy->FlushBarriers();

				auto dst = Utils::TextureCopy(*t, 0u);
				auto src = Utils::TextureCopy(*staging, t->BackingResource()->Footprints(0));
				copy->CommandList()->CopyTextureRegion(&dst, 0u, 0u, 0u, &src, nullptr);

				for (auto i = 0u; i < sky->BackingResource()->Desc.DepthOrArraySize; ++i)
				{
					auto d = Utils::TextureCopy(*sky, i * sky->BackingResource()->Desc.MipLevels);
					auto s = Utils::TextureCopy(*staging, sky->BackingResource()->Footprints(0));
					copy->CommandList()->CopyTextureRegion(&d, 0u, 0u, 0u, &s, nullptr);
				}

				copy->Transition(sky, ResourceCurrentStage::Idle);
				copy->Transition(t,   ResourceCurrentStage::Idle);
				copy->FlushBarriers();
			});

		BufferUploader vboW{ Meshes::Generator::Generate(1.414f, 1.0f) };
		vbo->Write(vboW);

		device.GetCommandContext()->Commit();
	}

	void Create_Pipeline()
	{
		rso =
			RootSignatureBuilder{}
			.AddRoot(::D3D12_ROOT_PARAMETER_TYPE_CBV, 0u, 0u, D3D12_SHADER_VISIBILITY_VERTEX)
			.BeginTable()
			.AddRange(::D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, 0u, 0u)
			.AddRange(::D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, 1u, 0u)
			.EndTable(::D3D12_SHADER_VISIBILITY_PIXEL)
			.AddRoot(::D3D12_ROOT_PARAMETER_TYPE_SRV, 2u, 0u, D3D12_SHADER_VISIBILITY_VERTEX)
			.AddStaticSampler(Transform(NormalWrap, 0u, 0u, ::D3D12_SHADER_VISIBILITY_PIXEL))
			.Create(device, 0u, ::D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		//TEMPORARY.
		::std::unique_ptr<ShaderManager> shaderManager{ ::std::make_unique<ShaderManager>() };
		::std::unique_ptr<PipelineManager> pipeManager{ ::std::make_unique<PipelineManager>() };

		shaderManager->ReadFile(L"projection.hlsl");
		pvs = shaderManager->Obtain<VertexShader>();
		pps = shaderManager->Obtain<PixelShader>();
		pvs->Link(pps);

		auto builder = pipeManager->Build(rso.get());

		builder
			.Shaders(pvs.get())
			.DepthState(true, ::D3D12_COMPARISON_FUNC_LESS_EQUAL, ds->BackingResource()->Desc.Format)
			.RenderTargetState(0u, ::D3D12_COLOR_WRITE_ENABLE_ALL, rt->BackingResource()->Desc.Format)
			.InputLayout
			({
				{ "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
				{ "UV_COORD", 0u, DXGI_FORMAT_R32G32_FLOAT, 1u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u},
				})
				.SampleState(rt->BackingResource()->Desc.SampleDesc);

		gpso_p = pipeManager->Create(device, builder);

		shaderManager->ReadFile(L"skybox.hlsl");
		svs = shaderManager->Obtain<VertexShader>();
		sps = shaderManager->Obtain<PixelShader>();
		svs->Link(sps);

		builder
			.Shaders(svs.get())
			.DepthState(true, ::D3D12_COMPARISON_FUNC_LESS_EQUAL, ds->BackingResource()->Desc.Format)
			.InputLayout
			({ { "POSITION", 0u, ::DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u } })
			.RasterizerState({ .DepthClipEnable{true} });

		gpso_s = pipeManager->Create(device, builder);
	}

	void Create_RTDS(::IDXGISwapChain4* swapchain)
	{
		scb = ::std::make_shared<SwapchainBuffer>(swapchain);
		auto desc = scb->Desc();
		rt = device.Create<RenderBuffer>({ desc.Width, desc.Height, DXGI_SAMPLE_DESC{ 2u, 0u }, ::DXGI_FORMAT_R8G8B8A8_UNORM }, "RT");
		ds = device.Create<DepthStencil>({ desc.Width, desc.Height, DXGI_SAMPLE_DESC{ 2u, 0u }, ::DXGI_FORMAT_D32_FLOAT }, "DS");
	}

	void operator()(CommandContext::DirectCommandContext* dc)
	{
		auto vertexCount = vbo->View().front().SizeInBytes / vbo->View().front().StrideInBytes;
		static Camera cam{};
		static float fov{ Constants::pi / 5 };
		cam.Rotate(0.01f);
		cam.FOV(fov += 0.05f);
		cam.Update();
		auto value = cam.Retrieve<Camera::ViewProjection, Camera::Position>();

		cbo->Write(
			[&value](::std::span<::std::byte> w) -> ::UINT
			{
				::std::ranges::copy(
					::std::span{ ::std::bit_cast<::std::byte*>(&value), sizeof(value) },
					w.begin());

				return sizeof(value);
			});

		dc->Pipeline(gpso_p);
		dc->RootSignature(rso.get());

		dc->Bind(vbo, 2u);

		dc->Bind(t, pps->BindingDesc("NormalTexture"));
		dc->Bind(cbo, pvs->BindingDesc("Projection"));
		dc->Bind(sky, sps->BindingDesc("SkyBox"));

		dc->Bind(rt);
		dc->Bind(ds);

		dc->Transition(t, ::D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		dc->Transition(sky, ::D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		dc->Transition(rt, ::D3D12_RESOURCE_STATE_RENDER_TARGET);
		dc->Transition(ds, ::D3D12_RESOURCE_STATE_DEPTH_WRITE);

		dc->FlushBarriers();

		dc->Viewport(static_cast<LONG>(rt->BackingResource()->Desc.Width), static_cast<LONG>(rt->BackingResource()->Desc.Height));
		dc->ClearTarget(true, true);

		dc->Draw({ .VertexCountPerInstance{vertexCount}, .Topology{D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST}, });

		dc->Pipeline(gpso_s);

		dc->ClearTarget();

		dc->Draw({ .VertexCountPerInstance{vertexCount}, .Topology{D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST}, });

		dc->Copy(rt, scb);

		dc->Transition(t, ResourceCurrentStage::Idle);
		dc->Transition(sky, ResourceCurrentStage::Idle);
		dc->Transition(rt, ResourceCurrentStage::Idle);
		dc->Transition(ds, ResourceCurrentStage::Idle);

		dc->FlushBarriers();
	}

	Device& device{ *EnumAdapters(0u)->GetDevice() };

	template<typename T> using ObjectPtr = ::std::shared_ptr<T>;

	ObjectPtr<VertexBuffer> vbo{ device.Create<VertexBuffer>({2048u}, "Vbo") };
	ObjectPtr<TextureCube> sky{ device.Create<TextureCube>({128u, 128u, ::DXGI_FORMAT_R8G8B8A8_UNORM}, "Sky") };
	ObjectPtr<Texture> t{ device.Create<Texture>({128u, 128u, ::DXGI_FORMAT_R8G8B8A8_UNORM}, "Texture") };
	ObjectPtr<ConstantBuffer> cbo{ device.Create<ConstantBuffer>({128u}, "Cbo") };
	GraphicsPipelineState* gpso_p;
	GraphicsPipelineState* gpso_s;
	ObjectPtr<RootSignature> rso;
	ObjectPtr<SwapchainBuffer> scb;
	ObjectPtr<DepthStencil> ds;
	ObjectPtr<RenderBuffer> rt;

	ObjectPtr<VertexShader> pvs;
	ObjectPtr<PixelShader>  pps;

	ObjectPtr<VertexShader> svs;
	ObjectPtr<PixelShader>  sps;

};

// One possible version to invoke this item.
// 
// WARNING: 
// 1. assume we have an IDXGISwapChain4* instance named (swapChain).
// 2. compiling shader wont change current directory, 
//    so make sure these hlsl is inside your working directory.
// 
// void Render()
// {
//		auto context{ EnumAdapters(0)->GetDevice()->GetCommandContext() };
//		
//		// for example, make this static...
//		static Sample_SkyBox_Projection pro{ swapChain };
// 
//		// Because it is the only item we need to render. Just run on main.
//		context->ExecuteOnMain(pro);
//		context->Commit();
//		
//		// This library wont call Present() automatically.
//		swapChain->Present(...);
// }
//	
//	And you will see an cube with gradient color surrounding by an same texture's skybox.
//	