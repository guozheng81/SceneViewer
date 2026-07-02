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

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> SrvRanges;
	CMaterial::IntRootParameters(0, 1, 0, RootParams, SrvRanges);
	Material.Build(L"PostProcessing_VSMain.cso", L"PostProcessing_PSMain.cso", RootParams);
}

void CLightPass::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	//Material.OnRender(InCommandList);

	//CScreenPass::OnRender(InCommandList);
}
