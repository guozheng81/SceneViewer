#include "ScreenPass.h"
#include "Renderer.h"
#include "Scene.h"

void CScreenPass::Init()
{
	ScreenQuad = CRenderer::GetInstance().GetScreenQuad();
}

void CScreenPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
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
	CMaterial::IntRootParameters(1, 5, 0, 0, RootParams, SrvRanges);
	Material.BuildRootSignature(RootParams, false);
	Material.BuildPSO(L"ScreenPass_VSMain.cso", L"ScreenPass_PSLighting.cso");

	GBufferA = CRenderer::GetInstance().GetTexture("GBufferA");
	GBufferB = CRenderer::GetInstance().GetTexture("GBufferB");
	Depth = CRenderer::GetInstance().GetTexture("Depth");

	ShadowRT = CRenderer::GetInstance().GetTexture("ShadowRT");

	IndirectLightRT = CRenderer::GetInstance().GetTexture("ATrous0");
}

void CLightPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	CRenderer::GetInstance().ResourceBarrier(CRenderer::GetInstance().GetCurrentFrameContext().FrameBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	CRenderer::GetInstance().ResourceBarrier(GBufferA->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	Material.OnRender(InCommandList);

	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle = CRenderer::GetInstance().GetCurrentFrameContext().FrameBufferRtvDescriptor;
	InCommandList->OMSetRenderTargets(1, &RtvHandle, false, nullptr);

	float ClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	InCommandList->ClearRenderTargetView(RtvHandle, ClearColor, 0, nullptr);

	InCommandList->SetGraphicsRootConstantBufferView(0, CRenderer::GetInstance().GetCurrentFrameContext().ViewBuffer.GetGPUAddress());
	Material.SetShaderResource(InCommandList, 0, GBufferA);
	Material.SetShaderResource(InCommandList, 1, GBufferB);
	Material.SetShaderResource(InCommandList, 2, Depth);

	Material.SetShaderResource(InCommandList, 3, ShadowRT);
	Material.SetShaderResource(InCommandList, 4, IndirectLightRT);

	CScreenPass::OnRender(InCommandList);
}

void CSimpleRTPass::Init()
{
	CScreenPass::Init();

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> Ranges;
	CMaterial::IntRootParameters(1, 2, 1, 2, RootParams, Ranges);

	Material.BuildRootSignature(RootParams, true);
	std::vector<SRaytracingShaderInfo> ShaderInfoArray(1);
	ShaderInfoArray[0].MissShader = L"PrimaryMiss";
	ShaderInfoArray[0].HitGroup = L"PrimaryHitGroup";
	ShaderInfoArray[0].ClosestHitShader = L"PrimaryClosestHit";
	ShaderInfoArray[0].AnyHitShader = L"PrimaryAnyHit";

	Material.BuildRaytracingPSO(L"SimpleRT.cso", L"PrimaryRayGen", ShaderInfoArray);

	SimpleRT = CRenderer::GetInstance().CreateRenderTarget("SimpleRT", DXGI_FORMAT_R8G8B8A8_UNORM, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);

}

void CSimpleRTPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	CRenderer::GetInstance().ResourceBarrier(SimpleRT->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	Material.OnRender(InCommandList);
	InCommandList->SetComputeRootConstantBufferView(0, CRenderer::GetInstance().GetCurrentFrameContext().ViewBuffer.GetGPUAddress());

	Material.SetSceneForRaytracing(InCommandList, CRenderer::GetInstance().GetScene());

	Material.SetUav(InCommandList, 0, SimpleRT);

	// dispatch raytracing
	InCommandList->DispatchRays(&(Material.RaytraceDesc));

	CRenderer::GetInstance().ResourceBarrier(SimpleRT->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(SimpleRT->GetResource());
	InCommandList->ResourceBarrier(1, &UavBarrier);
}

void CShadowRTPass::Init()
{
	CScreenPass::Init();

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> Ranges;
	CMaterial::IntRootParameters(1, 4, 1, 2, RootParams, Ranges);

	Material.BuildRootSignature(RootParams, true);
	std::vector<SRaytracingShaderInfo> ShaderInfoArray(1);
	ShaderInfoArray[0].MissShader = L"ShadowMiss";
	ShaderInfoArray[0].HitGroup = L"ShadowHitGroup";
	ShaderInfoArray[0].ClosestHitShader = L"ShadowClosestHit";
	ShaderInfoArray[0].AnyHitShader = L"ShadowAnyHit";

	Material.BuildRaytracingPSO(L"ShadowRT.cso", L"ShadowRayGen", ShaderInfoArray);

	ShadowRT = CRenderer::GetInstance().CreateRenderTarget("ShadowRT", DXGI_FORMAT_R8_UNORM, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
	GBufferB = CRenderer::GetInstance().GetTexture("GBufferB");
	Depth = CRenderer::GetInstance().GetTexture("Depth");
}

void CShadowRTPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	CRenderer::GetInstance().ResourceBarrier(ShadowRT->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	Material.OnRender(InCommandList);
	InCommandList->SetComputeRootConstantBufferView(0, CRenderer::GetInstance().GetCurrentFrameContext().ViewBuffer.GetGPUAddress());

	Material.SetShaderResource(InCommandList, 2, GBufferB);
	Material.SetShaderResource(InCommandList, 3, Depth);
	Material.SetSceneForRaytracing(InCommandList, CRenderer::GetInstance().GetScene());

	Material.SetUav(InCommandList, 0, ShadowRT);

	// dispatch raytracing
	InCommandList->DispatchRays(&(Material.RaytraceDesc));

	CRenderer::GetInstance().ResourceBarrier(ShadowRT->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(ShadowRT->GetResource());
	InCommandList->ResourceBarrier(1, &UavBarrier);
}

void CIndirectLightRTPass::Init()
{
	CScreenPass::Init();

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> Ranges;
	CMaterial::IntRootParameters(1, 4, 1, 2, RootParams, Ranges);

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

	Material.BuildRaytracingPSO(L"IndirectLightRT.cso", L"IndirectRayGen", ShaderInfoArray, 2);

	IndirectLightRT = CRenderer::GetInstance().CreateRenderTarget("IndirectLightRT", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
	GBufferB = CRenderer::GetInstance().GetTexture("GBufferB");
	Depth = CRenderer::GetInstance().GetTexture("Depth");

	///////////////////

	std::vector<CD3DX12_ROOT_PARAMETER>	TARootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> TARanges;
	CMaterial::IntRootParameters(1, 3, 1, 0, TARootParams, TARanges);

	TemporalAccumulate.BuildRootSignature(TARootParams, false);
	TemporalAccumulate.BuildComputePSO(L"TemporalAccumulate.cso");
	TA0 = CRenderer::GetInstance().CreateRenderTarget("TA0", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
	TA1 = CRenderer::GetInstance().CreateRenderTarget("TA1", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);

	//////////////////

	std::vector<CD3DX12_ROOT_PARAMETER>	ATrousRootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> ATrousRanges;
	CMaterial::IntRootParameters(0, 3, 1, 0, ATrousRootParams, ATrousRanges);
	CD3DX12_ROOT_PARAMETER RootParam;
	RootParam.InitAsConstants(sizeof(SATrousConstants)/4, 0);
	ATrousRootParams.push_back(RootParam);

	ATrousMaterial.BuildRootSignature(ATrousRootParams, false);
	ATrousMaterial.BuildComputePSO(L"EdgeAvoidATrous.cso");

	ATrous0 = CRenderer::GetInstance().CreateRenderTarget("ATrous0", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
	ATrous1 = CRenderer::GetInstance().CreateRenderTarget("ATrous1", DXGI_FORMAT_R32G32B32A32_FLOAT, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);
}

void CIndirectLightRTPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	CRenderer::GetInstance().ResourceBarrier(IndirectLightRT->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	Material.OnRender(InCommandList);
	InCommandList->SetComputeRootConstantBufferView(0, CRenderer::GetInstance().GetCurrentFrameContext().ViewBuffer.GetGPUAddress());

	Material.SetShaderResource(InCommandList, 2, GBufferB);
	Material.SetShaderResource(InCommandList, 3, Depth);
	Material.SetSceneForRaytracing(InCommandList, CRenderer::GetInstance().GetScene());

	Material.SetUav(InCommandList, 0, IndirectLightRT);

	// dispatch raytracing
	InCommandList->DispatchRays(&(Material.RaytraceDesc));

	CRenderer::GetInstance().ResourceBarrier(IndirectLightRT->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(IndirectLightRT->GetResource());
	InCommandList->ResourceBarrier(1, &UavBarrier);

	////////////// Temporal accumulate

	CTexture2D* TATarget = (bIsTA1Target? TA1 : TA0);
	CTexture2D* TASource = (bIsTA1Target ? TA0 : TA1);

	{
		if (CRenderer::GetInstance().IsFristFrame())
		{
			TASource = IndirectLightRT;
		}

		TemporalAccumulate.OnRender(InCommandList);

		CRenderer::GetInstance().ResourceBarrier(TATarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		InCommandList->SetComputeRootConstantBufferView(0, CRenderer::GetInstance().GetCurrentFrameContext().ViewBuffer.GetGPUAddress());
		TemporalAccumulate.SetShaderResource(InCommandList, 0, IndirectLightRT);
		TemporalAccumulate.SetShaderResource(InCommandList, 1, TASource);
		TemporalAccumulate.SetShaderResource(InCommandList, 2, Depth);
		TemporalAccumulate.SetUav(InCommandList, 0, TATarget);

		InCommandList->Dispatch((CRenderer::GetInstance().ViewportWidth + 8) / 8, (CRenderer::GetInstance().ViewportHeight + 8) / 8, 1);

		CRenderer::GetInstance().ResourceBarrier(TATarget->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(TATarget->GetResource());
		InCommandList->ResourceBarrier(1, &UavBarrier);
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
		CTexture2D* TargetTexture = (bUse0AsTarget ? ATrous0 : ATrous1);
		CTexture2D* SrcTexture = (bUse0AsTarget ? ATrous1 : ATrous0);
		if (ATrousIdx == 0)
		{
			SrcTexture = TATarget;
		}

		CRenderer::GetInstance().ResourceBarrier(TargetTexture->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		ATrousConstants.g_StepSize = pow(2, ATrousIdx);
		InCommandList->SetComputeRoot32BitConstants(ATrousMaterial.FindConstantRootParameterIndex(0), sizeof(SATrousConstants) / 4, &ATrousConstants, 0);

		ATrousMaterial.SetShaderResource(InCommandList, 0, SrcTexture);
		ATrousMaterial.SetUav(InCommandList, 0, TargetTexture);

		InCommandList->Dispatch((CRenderer::GetInstance().ViewportWidth + 15) / 16, (CRenderer::GetInstance().ViewportHeight + 15) / 16, 1);

		CRenderer::GetInstance().ResourceBarrier(TargetTexture->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(TargetTexture->GetResource());
		InCommandList->ResourceBarrier(1, &UavBarrier);
	}
	
}

