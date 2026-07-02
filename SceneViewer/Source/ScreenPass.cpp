#include "ScreenPass.h"
#include "Renderer.h"

void CScreenPass::Init()
{
	ScreenQuad = CRenderer::GetInstance().GetScreenQuad();
}

void CScreenPass::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	if (ScreenQuad)
	{
		ScreenQuad->OnRender(InCommandList);
	}
}

void CLightPass::Init()
{
	CScreenPass::Init();

	Material.PSODesc.DepthStencilState.DepthEnable = false;
	Material.PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> SrvRanges;
	CMaterial::IntRootParameters(0, 1, 0, RootParams, SrvRanges);
	Material.Build(L"PostProcessing_VSMain.cso", L"PostProcessing_PSMain.cso", RootParams);

	GBufferA = CRenderer::GetInstance().GetTexture("GBufferA");
}

void CLightPass::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	CRenderer::GetInstance().ResourceBarrier(GBufferA->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	Material.OnRender(InCommandList);

	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle = CRenderer::GetInstance().GetCurrentFrameContext().FrameBufferRtvDescriptor;
	InCommandList->OMSetRenderTargets(1, &RtvHandle, false, nullptr);

	float ClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	InCommandList->ClearRenderTargetView(RtvHandle, ClearColor, 0, nullptr);

	Material.SetShaderResource(InCommandList, 0, GBufferA);

	CScreenPass::OnRender(InCommandList);
}
