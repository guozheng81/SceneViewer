#pragma once

#include "Utils.h"

class CMesh
{
protected:
	UINT	IndicesCount = 0;
	UINT	VertexCount = 0;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW	 IndexBufferView;

	ComPtr<ID3D12Resource> VertexBuffer;
	ComPtr<ID3D12Resource> IndexBuffer;

	ComPtr<ID3D12Resource> VertexUploadBuffer;
	ComPtr<ID3D12Resource> IndexUploadBuffer;

	XMMATRIX WorldMatrix = XMMatrixIdentity();

public:

	void Init(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices);
	void ResetUploadResource();

	void	GetWorldMatrix(XMFLOAT4X4* OutMtx);

	void OnRender(ID3D12GraphicsCommandList* InCommandList);
};


