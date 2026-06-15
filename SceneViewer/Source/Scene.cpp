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
	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc;
	RootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Material = std::make_shared<CMaterial>(L"Scene_VSMain.cso", L"Scene_PSMain.cso", &RootSignatureDesc);

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
	Material->OnRender(InCommandList);

	for (auto CurMesh : AllMeshes)
	{
		CurMesh->OnRender(InCommandList);
	}
}