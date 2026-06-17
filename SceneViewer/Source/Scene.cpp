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

	Material = std::make_shared<CMaterial>();
	Material->PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	CD3DX12_ROOT_PARAMETER	RootParams[1];
	RootParams[0].InitAsConstantBufferView(0);

	Material->RootSignatureDesc.Init(1, RootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	Material->Build(L"Scene_VSMain.cso", L"Scene_PSMain.cso");

	std::shared_ptr<CMesh> CurMesh = std::make_shared<CMesh>();
	AllMeshes.push_back(CurMesh);

	std::vector<SSceneVertex> Verts = {
			{ { 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
			{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} }
	};

	std::vector<UINT32>	Indices = { 0, 1, 2 };

	CurMesh->Init(Verts, Indices);
}

CScene::~CScene()
{

}

void CScene::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	for (auto CurMesh : AllMeshes)
	{
		CurMesh->OnRender(InCommandList);
	}
}