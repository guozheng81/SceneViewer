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
	CMaterial::IntRootParameters(1, 4, 0, RootParams, SrvRanges);
	Material.BuildRootSignature(RootParams, false);
	Material.BuildPSO(L"ScreenPass_VSMain.cso", L"ScreenPass_PSLighting.cso");

	GBufferA = CRenderer::GetInstance().GetTexture("GBufferA");
	GBufferB = CRenderer::GetInstance().GetTexture("GBufferB");
	Depth = CRenderer::GetInstance().GetTexture("Depth");

	SimpleRT = CRenderer::GetInstance().GetTexture("SimpleRT");
}

void CLightPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	CRenderer::GetInstance().ResourceBarrier(CRenderer::GetInstance().GetCurrentFrameContext().FrameBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	CRenderer::GetInstance().ResourceBarrier(GBufferA->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	CRenderer::GetInstance().ResourceBarrier(GBufferB->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	CRenderer::GetInstance().ResourceBarrier(Depth->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	Material.OnRender(InCommandList);

	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle = CRenderer::GetInstance().GetCurrentFrameContext().FrameBufferRtvDescriptor;
	InCommandList->OMSetRenderTargets(1, &RtvHandle, false, nullptr);

	float ClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	InCommandList->ClearRenderTargetView(RtvHandle, ClearColor, 0, nullptr);

	InCommandList->SetGraphicsRootConstantBufferView(0, CRenderer::GetInstance().GetCurrentFrameContext().ViewBuffer.GetGPUAddress());
	Material.SetShaderResource(InCommandList, 0, GBufferA);
	Material.SetShaderResource(InCommandList, 1, GBufferB);
	Material.SetShaderResource(InCommandList, 2, Depth);

	Material.SetShaderResource(InCommandList, 3, SimpleRT);

	CScreenPass::OnRender(InCommandList);
}

void CSimpleRTPass::Init()
{
	CScreenPass::Init();

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> Ranges;
	CMaterial::IntRootParameters(1, 2, 1, RootParams, Ranges);

	CD3DX12_DESCRIPTOR_RANGE DescRange1;
	DescRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 0, 1);
	CD3DX12_ROOT_PARAMETER TexturesRootParam;
	TexturesRootParam.InitAsDescriptorTable(1, &DescRange1, D3D12_SHADER_VISIBILITY_ALL);
	RootParams.push_back(TexturesRootParam);

	CD3DX12_DESCRIPTOR_RANGE DescRange2;
	DescRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 0, 2);
	CD3DX12_ROOT_PARAMETER VertBuffersRootParam;
	VertBuffersRootParam.InitAsDescriptorTable(1, &DescRange2, D3D12_SHADER_VISIBILITY_ALL);
	RootParams.push_back(VertBuffersRootParam);

	Material.BuildRootSignature(RootParams, true);
	Material.BuildRaytracingPSO(L"SimpleRT.cso", L"PrimaryRayGen");

	SimpleRT = CRenderer::GetInstance().CreateRenderTarget("SimpleRT", DXGI_FORMAT_R8G8B8A8_UNORM, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, false, true);

	// miss and hitgroup start address need to align with 64, so we have empty entry in the table buffer
	ShaderBindingTable.Init(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, 3, true);
	ShaderBindingTable.SetElementData(0, Material.GetRaytracingShaderIdentifier(L"PrimaryRayGen"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	ShaderBindingTable.SetElementData(1, Material.GetRaytracingShaderIdentifier(L"PrimaryMiss"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	ShaderBindingTable.SetElementData(2, Material.GetRaytracingShaderIdentifier(L"PrimaryHitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
}

void CSimpleRTPass::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	CRenderer::GetInstance().ResourceBarrier(SimpleRT->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	Material.OnRender(InCommandList);
	InCommandList->SetComputeRootConstantBufferView(0, CRenderer::GetInstance().GetCurrentFrameContext().ViewBuffer.GetGPUAddress());

	Material.SetShaderResource(InCommandList, 0, CRenderer::GetInstance().GetScene()->GetModelBuffer());
	int TLASParam = Material.FindSrvRootParameterIndex(1);
	if (TLASParam >= 0)
	{
		InCommandList->SetComputeRootDescriptorTable(TLASParam, CRenderer::GetInstance().GetScene()->TLASGPUDescriptor);
	}

	int TexturesParam = Material.FindSrvRootParameterIndex(0, 1);
	if (TexturesParam >= 0)
	{
		InCommandList->SetComputeRootDescriptorTable(TexturesParam, CRenderer::GetInstance().GetScene()->MaterialTexturesDescriptor);
	}

	int VertexBufferParam = Material.FindSrvRootParameterIndex(0, 2);
	if (VertexBufferParam >= 0)
	{
		InCommandList->SetComputeRootDescriptorTable(VertexBufferParam, CRenderer::GetInstance().GetScene()->VertexBuffersDescriptor);
	}

	Material.SetUav(InCommandList, 0, SimpleRT);

	// dispatch raytracing
	D3D12_DISPATCH_RAYS_DESC RaytraceDesc = {};
	RaytraceDesc.Width = CRenderer::GetInstance().ViewportWidth;
	RaytraceDesc.Height = CRenderer::GetInstance().ViewportHeight;
	RaytraceDesc.Depth = 1;

	RaytraceDesc.RayGenerationShaderRecord.StartAddress = ShaderBindingTable.GetGPUAddress(0);
	RaytraceDesc.RayGenerationShaderRecord.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

	RaytraceDesc.MissShaderTable.StartAddress = ShaderBindingTable.GetGPUAddress(1);
	RaytraceDesc.MissShaderTable.StrideInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
	RaytraceDesc.MissShaderTable.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

	RaytraceDesc.HitGroupTable.StartAddress = ShaderBindingTable.GetGPUAddress(2);
	RaytraceDesc.HitGroupTable.StrideInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
	RaytraceDesc.HitGroupTable.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

	InCommandList->DispatchRays(&RaytraceDesc);

	CRenderer::GetInstance().ResourceBarrier(SimpleRT->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(SimpleRT->GetResource());
	InCommandList->ResourceBarrier(1, &UavBarrier);
}
