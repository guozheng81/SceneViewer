#include "Mesh.h"
#include "Renderer.h"

void CMesh::Init(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices)
{
	VertexCount = Verts.size();
	UINT TotalSize = sizeof(SSceneVertex) * VertexCount;
	VertexBuffer = CRenderer::GetInstance().CreateDefaultBuffer(Verts.data(), TotalSize, VertexUploadBuffer);

	VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
	VertexBufferView.SizeInBytes = TotalSize;
	VertexBufferView.StrideInBytes = sizeof(SSceneVertex);

	IndicesCount = Indices.size();
	if (IndicesCount > 0)
	{
		TotalSize = sizeof(UINT32) * IndicesCount;
		IndexBuffer = CRenderer::GetInstance().CreateDefaultBuffer(Indices.data(), TotalSize, IndexUploadBuffer);
		IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
		IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		IndexBufferView.SizeInBytes = TotalSize;
	}
}

void CMesh::ResetUploadResource()
{
	if (VertexUploadBuffer)
	{
		VertexUploadBuffer.Reset();
	}

	if (IndexUploadBuffer)
	{
		IndexUploadBuffer.Reset();
	}

}

void CMesh::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	InCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	InCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	if (IndicesCount > 0)
	{
		InCommandList->IASetIndexBuffer(&IndexBufferView);
		InCommandList->DrawIndexedInstanced(IndicesCount, 1, 0, 0, 0);
	}
	else
	{
		InCommandList->DrawInstanced(VertexCount, 1, 0, 0);
	}
}

void	CMesh::GetWorldMatrix(XMFLOAT4X4* OutMtx)
{
	XMStoreFloat4x4(OutMtx, XMMatrixTranspose(WorldMatrix));
}

D3D12_GPU_VIRTUAL_ADDRESS CMesh::GetVertexGPUAddress()
{
	if (VertexBuffer)
	{
		return	VertexBuffer->GetGPUVirtualAddress();
	}
	return 0;
}
