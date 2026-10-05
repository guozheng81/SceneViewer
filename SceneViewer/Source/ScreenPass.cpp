#include "ScreenPass.h"
#include "Renderer.h"
#include "Scene.h"
#include "Texture.h"

void CScreenPass::Init()
{
	ScreenQuad = CRenderer::GetInstance().GetScreenQuad();
}

void CScreenPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	CRenderer::GetInstance().BarrierBatcher.Flush(InCommandList);
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
	ComPtr<ID3DBlob> VSBlob = CMaterial::ReadShaderFile(L"ScreenPass_VSMain.cso");
	ComPtr<ID3DBlob> PSBlob = CMaterial::ReadShaderFile(L"ScreenPass_PSLighting.cso");
	CMaterial::InitRootParametersFromShaders({ VSBlob.Get(), PSBlob.Get() }, RootParams, SrvRanges);

	Material.BuildRootSignature(RootParams, false);
	Material.BuildPSO(VSBlob, PSBlob);

	GBufferA = dynamic_cast<CTextureRenderTarget*>(CRenderer::GetInstance().GetTexture("GBufferA"));
	GBufferB = dynamic_cast<CTextureRenderTarget*>(CRenderer::GetInstance().GetTexture("GBufferB"));

	ShadowRT = dynamic_cast<CTextureRenderTarget*>(CRenderer::GetInstance().GetTexture("ShadowRT"));

	IndirectLightRT = dynamic_cast<CTextureRenderTarget*>(CRenderer::GetInstance().GetTexture("ATrous0"));

	SHVolumeR = dynamic_cast<CTexture3D*>(CRenderer::GetInstance().GetTexture("SHVolumeR"));
	SHVolumeG = dynamic_cast<CTexture3D*>(CRenderer::GetInstance().GetTexture("SHVolumeG"));
	SHVolumeB = dynamic_cast<CTexture3D*>(CRenderer::GetInstance().GetTexture("SHVolumeB"));
}

void CLightPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	Depth = CRenderer::GetInstance().GetScene()->GetDepthTexture();

	Material.OnRender(InCommandList);

	CRenderer::GetInstance().SetFrameBufferRenderTarget();

	Material.SetConstantBuffer(InCommandList, 0, CRenderer::GetInstance().GetCurrentViewBuffer());
	Material.SetShaderResource(InCommandList, 0, GBufferA);
	Material.SetShaderResource(InCommandList, 1, GBufferB);
	Material.SetShaderResource(InCommandList, 2, Depth);

	Material.SetShaderResource(InCommandList, 3, ShadowRT);
	Material.SetShaderResource(InCommandList, 4, IndirectLightRT);

	Material.SetShaderResource(InCommandList, 5, SHVolumeR);
	Material.SetShaderResource(InCommandList, 6, SHVolumeG);
	Material.SetShaderResource(InCommandList, 7, SHVolumeB);

	CScreenPass::OnRender(InCommandList);
}

void CSimpleRTPass::Init()
{
	CScreenPass::Init();

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> Ranges;
	ComPtr<ID3DBlob> Blob = CMaterial::ReadShaderFile(L"SimpleRT.cso");
	CMaterial::InitRootParametersFromShaders({ Blob.Get() }, RootParams, Ranges);

	Material.BuildRootSignature(RootParams, true);
	std::vector<SRaytracingShaderInfo> ShaderInfoArray(1);
	ShaderInfoArray[0].MissShader = L"PrimaryMiss";
	ShaderInfoArray[0].HitGroup = L"PrimaryHitGroup";
	ShaderInfoArray[0].ClosestHitShader = L"PrimaryClosestHit";
	ShaderInfoArray[0].AnyHitShader = L"PrimaryAnyHit";

	Material.BuildRaytracingPSO(Blob, L"PrimaryRayGen", ShaderInfoArray);

	SimpleRT = CRenderer::GetInstance().CreateRenderTarget("SimpleRT", DXGI_FORMAT_R8G8B8A8_UNORM, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);

}

void CSimpleRTPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	Material.OnRender(InCommandList);
	Material.SetConstantBuffer(InCommandList, 0, CRenderer::GetInstance().GetCurrentViewBuffer());

	Material.SetSceneForRaytracing(InCommandList, CRenderer::GetInstance().GetScene());

	Material.SetUav(InCommandList, 0, SimpleRT);

	CRenderer::GetInstance().BarrierBatcher.Flush(InCommandList);
	// dispatch raytracing
	InCommandList->DispatchRays(&(Material.RaytraceDesc));

}

void CShadowRTPass::Init()
{
	CScreenPass::Init();

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> Ranges;
	ComPtr<ID3DBlob> ShaderBlob = CMaterial::ReadShaderFile(L"ShadowRT.cso");
	CMaterial::InitRootParametersFromShaders({ ShaderBlob.Get() }, RootParams, Ranges);

	Material.BuildRootSignature(RootParams, true);
	std::vector<SRaytracingShaderInfo> ShaderInfoArray(1);
	ShaderInfoArray[0].MissShader = L"ShadowMiss";
	ShaderInfoArray[0].HitGroup = L"ShadowHitGroup";
	ShaderInfoArray[0].ClosestHitShader = L"ShadowClosestHit";
	ShaderInfoArray[0].AnyHitShader = L"ShadowAnyHit";

	Material.BuildRaytracingPSO(ShaderBlob, L"ShadowRayGen", ShaderInfoArray);

	ShadowRT = CRenderer::GetInstance().CreateRenderTarget("ShadowRT", DXGI_FORMAT_R8_UNORM, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
	GBufferB = dynamic_cast<CTextureRenderTarget*>(CRenderer::GetInstance().GetTexture("GBufferB"));
}

void CShadowRTPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	Depth = CRenderer::GetInstance().GetScene()->GetDepthTexture();

	Material.OnRender(InCommandList);
	Material.SetConstantBuffer(InCommandList, 0, CRenderer::GetInstance().GetCurrentViewBuffer());

	Material.SetShaderResource(InCommandList, 2, GBufferB);
	Material.SetShaderResource(InCommandList, 3, Depth);
	Material.SetSceneForRaytracing(InCommandList, CRenderer::GetInstance().GetScene());

	Material.SetUav(InCommandList, 0, ShadowRT);

	CRenderer::GetInstance().BarrierBatcher.Flush(InCommandList);
	// dispatch raytracing
	InCommandList->DispatchRays(&(Material.RaytraceDesc));
}

void CIndirectLightRTPass::Init()
{
	CScreenPass::Init();

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> Ranges;
	ComPtr<ID3DBlob> ShaderBlob = CMaterial::ReadShaderFile(L"IndirectLightRT.cso");
	CMaterial::InitRootParametersFromShaders({ ShaderBlob.Get() }, RootParams, Ranges);

	Material.BuildRootSignature(RootParams, true);
	std::vector<SRaytracingShaderInfo> ShaderInfoArray(2);
	ShaderInfoArray[0].MissShader = L"IndirectMiss";
	ShaderInfoArray[0].HitGroup = L"IndirectHitGroup";
	ShaderInfoArray[0].ClosestHitShader = L"IndirectClosestHit";
	ShaderInfoArray[0].AnyHitShader = L"IndirectAnyHit";

	ShaderInfoArray[1].MissShader = L"ShadowMiss";
	ShaderInfoArray[1].HitGroup = L"ShadowHitGroup";
	ShaderInfoArray[1].ClosestHitShader = L"ShadowClosestHit";
	ShaderInfoArray[1].AnyHitShader = L"ShadowAnyHit";

	Material.BuildRaytracingPSO(ShaderBlob, L"IndirectRayGen", ShaderInfoArray, 2);

	IndirectLightRT = CRenderer::GetInstance().CreateRenderTarget("IndirectLightRT", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
	GBufferB = dynamic_cast<CTextureRenderTarget*>(CRenderer::GetInstance().GetTexture("GBufferB"));

	SHVolumeR = dynamic_cast<CTexture3D*>(CRenderer::GetInstance().GetTexture("SHVolumeR"));
	SHVolumeG = dynamic_cast<CTexture3D*>(CRenderer::GetInstance().GetTexture("SHVolumeG"));
	SHVolumeB = dynamic_cast<CTexture3D*>(CRenderer::GetInstance().GetTexture("SHVolumeB"));

	///////////////////

	std::vector<CD3DX12_ROOT_PARAMETER>	TARootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> TARanges;
	ComPtr<ID3DBlob> TABlob = CMaterial::ReadShaderFile(L"TemporalAccumulate.cso");
	CMaterial::InitRootParametersFromShaders({ TABlob.Get() }, TARootParams, TARanges);

	TemporalAccumulate.BuildRootSignature(TARootParams, false);
	TemporalAccumulate.BuildComputePSO(TABlob);
	TA0 = CRenderer::GetInstance().CreateRenderTarget("TA0", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
	TA1 = CRenderer::GetInstance().CreateRenderTarget("TA1", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);

	//////////////////

	std::vector<CD3DX12_ROOT_PARAMETER>	ATrousRootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> ATrousRanges;

	ComPtr<ID3DBlob> ATrousBlob = CMaterial::ReadShaderFile(L"EdgeAvoidATrous.cso");
	CMaterial::InitRootParametersFromShaders({ ATrousBlob.Get() }, ATrousRootParams, ATrousRanges);

	ATrousMaterial.BuildRootSignature(ATrousRootParams, false);
	ATrousMaterial.BuildComputePSO(ATrousBlob);

	ATrous0 = CRenderer::GetInstance().CreateRenderTarget("ATrous0", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
	ATrous1 = CRenderer::GetInstance().CreateRenderTarget("ATrous1", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
}

void CIndirectLightRTPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	Depth = CRenderer::GetInstance().GetScene()->GetDepthTexture();

	Material.OnRender(InCommandList);
	Material.SetConstantBuffer(InCommandList, 0, CRenderer::GetInstance().GetCurrentViewBuffer());

	Material.SetShaderResource(InCommandList, 2, GBufferB);
	Material.SetShaderResource(InCommandList, 3, Depth);
	Material.SetSceneForRaytracing(InCommandList, CRenderer::GetInstance().GetScene());

	Material.SetShaderResource(InCommandList, 4, SHVolumeR);
	Material.SetShaderResource(InCommandList, 5, SHVolumeG);
	Material.SetShaderResource(InCommandList, 6, SHVolumeB);

	Material.SetUav(InCommandList, 0, IndirectLightRT);

	CRenderer::GetInstance().BarrierBatcher.Flush(InCommandList);
	// dispatch raytracing
	InCommandList->DispatchRays(&(Material.RaytraceDesc));

	////////////// Temporal accumulate

	CTextureRenderTarget* TATarget = (bIsTA1Target? TA1 : TA0);
	CTextureRenderTarget* TASource = (bIsTA1Target ? TA0 : TA1);

	{
		if (CRenderer::GetInstance().IsFristFrame())
		{
			TASource = IndirectLightRT;
		}

		TemporalAccumulate.OnRender(InCommandList);

		TemporalAccumulate.SetConstantBuffer(InCommandList, 0, CRenderer::GetInstance().GetCurrentViewBuffer());
		TemporalAccumulate.SetShaderResource(InCommandList, 0, IndirectLightRT);
		TemporalAccumulate.SetShaderResource(InCommandList, 1, TASource);
		TemporalAccumulate.SetShaderResource(InCommandList, 2, Depth);
		TemporalAccumulate.SetShaderResource(InCommandList, 3, CRenderer::GetInstance().GetScene()->GetHistoryDepthTexture());
		TemporalAccumulate.SetUav(InCommandList, 0, TATarget);

		CRenderer::GetInstance().BarrierBatcher.Flush(InCommandList);
		InCommandList->Dispatch((CRenderer::GetInstance().ViewportWidth + 7) / 8, (CRenderer::GetInstance().ViewportHeight + 7) / 8, 1);
	}

	bIsTA1Target = !bIsTA1Target;

	////////////// ATrous ///////

	CCamera* Cam = CRenderer::GetInstance().GetScene()->GetMainCamera();
	float Near = Cam->GetNearPlane();
	float Far = Cam->GetFarPlane();

	ATrousConstants.Proj_m22 = Far / (Far - Near);
	ATrousConstants.Proj_m32 = (-Far) * Near / (Far - Near);
	ATrousConstants.ViewportWidth = CRenderer::GetInstance().ViewportWidth;
	ATrousConstants.ViewportHeight = CRenderer::GetInstance().ViewportHeight;

	ATrousMaterial.OnRender(InCommandList);
	ATrousMaterial.SetShaderResource(InCommandList, 1, GBufferB);
	ATrousMaterial.SetShaderResource(InCommandList, 2, Depth);

	for(int ATrousIdx = 0; ATrousIdx < 5; ++ATrousIdx)
	{
		bool bUse0AsTarget = (ATrousIdx % 2 == 0);
		CTextureRenderTarget* TargetTexture = (bUse0AsTarget ? ATrous0 : ATrous1);
		CTextureRenderTarget* SrcTexture = (bUse0AsTarget ? ATrous1 : ATrous0);
		if (ATrousIdx == 0)
		{
			SrcTexture = TATarget;
		}

		ATrousConstants.g_StepSize = pow(2, ATrousIdx);
		InCommandList->SetComputeRoot32BitConstants(ATrousMaterial.FindConstantRootParameterIndex(0), sizeof(SATrousConstants) / 4, &ATrousConstants, 0);

		ATrousMaterial.SetShaderResource(InCommandList, 0, SrcTexture);
		ATrousMaterial.SetUav(InCommandList, 0, TargetTexture);

		CRenderer::GetInstance().BarrierBatcher.Flush(InCommandList);
		InCommandList->Dispatch((CRenderer::GetInstance().ViewportWidth + 15) / 16, (CRenderer::GetInstance().ViewportHeight + 15) / 16, 1);
	}	
}

void CIrradianceVolumeRTPass::Init()
{
	CScreenPass::Init();

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> Ranges;
	ComPtr<ID3DBlob> ShaderBlob = CMaterial::ReadShaderFile(L"IrradianceVolumeRT.cso");
	CMaterial::InitRootParametersFromShaders({ ShaderBlob.Get() }, RootParams, Ranges);

	Material.BuildRootSignature(RootParams, true);

	std::vector<SRaytracingShaderInfo> ShaderInfoArray(2);
	ShaderInfoArray[0].MissShader = L"IrradianceVolumeMiss";
	ShaderInfoArray[0].HitGroup = L"IrradianceVolumeHitGroup";
	ShaderInfoArray[0].ClosestHitShader = L"IrradianceVolumeClosestHit";
	ShaderInfoArray[0].AnyHitShader = L"IrradianceVolumeAnyHit";

	ShaderInfoArray[1].MissShader = L"ShadowMiss";
	ShaderInfoArray[1].HitGroup = L"ShadowHitGroup";
	ShaderInfoArray[1].ClosestHitShader = L"ShadowClosestHit";
	ShaderInfoArray[1].AnyHitShader = L"ShadowAnyHit";

	Material.BuildRaytracingPSO(ShaderBlob, L"IrradianceVolumeRayGen", ShaderInfoArray, 2);

	SHVolumeR = CRenderer::GetInstance().CreateTexture3D("SHVolumeR", DXGI_FORMAT_R16G16B16A16_FLOAT, VolumeWidth, VolumeHeight, VolumeDepth);
	SHVolumeG = CRenderer::GetInstance().CreateTexture3D("SHVolumeG", DXGI_FORMAT_R16G16B16A16_FLOAT, VolumeWidth, VolumeHeight, VolumeDepth);
	SHVolumeB = CRenderer::GetInstance().CreateTexture3D("SHVolumeB", DXGI_FORMAT_R16G16B16A16_FLOAT, VolumeWidth, VolumeHeight, VolumeDepth);
}

void CIrradianceVolumeRTPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	Material.OnRender(InCommandList);
	Material.SetConstantBuffer(InCommandList, 0, CRenderer::GetInstance().GetCurrentViewBuffer());

	Material.SetSceneForRaytracing(InCommandList, CRenderer::GetInstance().GetScene());

	Material.SetUav(InCommandList, 0, SHVolumeR);
	Material.SetUav(InCommandList, 1, SHVolumeG);
	Material.SetUav(InCommandList, 2, SHVolumeB);

	CRenderer::GetInstance().BarrierBatcher.Flush(InCommandList);
	// dispatch raytracing
	Material.RaytraceDesc.Width = VolumeWidth;
	Material.RaytraceDesc.Height = VolumeHeight;
	Material.RaytraceDesc.Depth = VolumeDepth;
	InCommandList->DispatchRays(&(Material.RaytraceDesc));

}