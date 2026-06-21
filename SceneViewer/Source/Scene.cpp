#include "Scene.h"
#include "Renderer.h"

void CMesh::Init(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices)
{
	UINT TotalSize = sizeof(SSceneVertex)*Verts.size();
	VertexBuffer = CRenderer::GetInstance().CreateDefaultBuffer(Verts.data(), TotalSize, VertexUploadBuffer);

	VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
	VertexBufferView.SizeInBytes = TotalSize;
	VertexBufferView.StrideInBytes = sizeof(SSceneVertex);

	IndicesCount = Indices.size();
	TotalSize = sizeof(UINT32)*IndicesCount;
	IndexBuffer = CRenderer::GetInstance().CreateDefaultBuffer(Indices.data(), TotalSize, IndexUploadBuffer);
	IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = TotalSize;
}

void CMesh::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	InCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	InCommandList->IASetIndexBuffer(&IndexBufferView);
	InCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	InCommandList->DrawIndexedInstanced(IndicesCount, 1, 0, 0, 0);
}

CScene::CScene()
{
	MainCamera.SetAspectRatio(CRenderer::GetInstance().ViewportWidth, CRenderer::GetInstance().ViewportHeight);
	MainCamera.SetFOV(55.0f);
	MainCamera.SetPositionAndRotation(XMFLOAT3(0.0f, 0.0f, -10.0f), 0.0f, 0.0f);

	Material = std::make_unique<CMaterial>();
	//Material->PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
}

void CScene::Load()
{
	CD3DX12_DESCRIPTOR_RANGE TexRange;
	TexRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams(2);
	RootParams[0].InitAsConstantBufferView(0);
	RootParams[1].InitAsDescriptorTable(1, &TexRange, D3D12_SHADER_VISIBILITY_PIXEL);

	auto& Samplers = CRenderer::GetInstance().TextureSamplers;
	Material->RootSignatureDesc.Init((UINT)(RootParams.size()), RootParams.data(), (UINT)(Samplers.size()), Samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	Material->Build(L"Scene_VSMain.cso", L"Scene_PSMain.cso");

	CRenderer::GetInstance().LoadTexture(L"spnza_bricks_a_diff.dds");

	std::unique_ptr<CMesh> CurMesh = std::make_unique<CMesh>();

	std::vector<SSceneVertex> Verts = {
			{ { 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
			{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} }
	};

	std::vector<UINT32>	Indices = { 0, 1, 2 };
	CurMesh->Init(Verts, Indices);

	AllMeshes.push_back(std::move(CurMesh));
}

CMaterial* CScene::GetSceneMaterial()
{
	return Material.get();
}

CScene::~CScene()
{

}

void CScene::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	InCommandList->SetGraphicsRootDescriptorTable(1, CRenderer::GetInstance().GetSrvGPUDescriptor(0));
	for (auto& CurMesh : AllMeshes)
	{
		CurMesh->OnRender(InCommandList);
	}
}