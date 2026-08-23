#pragma once

#include "Utils.h"
#include "Renderer.h"

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

	// Instance data storage
	std::vector<XMMATRIX> InstanceWorldMatrices;

	bool bNeedsAlphaTest = false;

	int GlobalInstanceIndex = 0;

public:
	CD3DX12_GPU_DESCRIPTOR_HANDLE VertexSrvGPUDescriptor = {};

	void Init(const std::vector<SSceneVertex>& Verts, const std::vector<UINT32>& Indices, int InGlobalInstIdx = 0, bool bAlphaTest = false);
	void ResetUploadResource();

	// Instance management
	UINT AddInstance(const XMMATRIX& InWorldMatrix);
	void SetInstanceWorldMatrix(UINT InstanceIndex, const XMMATRIX& InWorldMatrix);
	void GetInstanceWorldMatrix(UINT InstanceIndex, XMFLOAT4X4* OutMtx);
	void ClearInstances();
	UINT GetInstanceCount() const;
	int GetGlobalInstanceIndex() const { return GlobalInstanceIndex; }

	// Deprecated - kept for compatibility
	void	GetWorldMatrix(XMFLOAT4X4* OutMtx);

	void OnRender(ID3D12GraphicsCommandList* InCommandList);

	inline UINT GetVertexCount() const {	return VertexCount;	}
	inline UINT GetIndicesCount() const {	return IndicesCount;	}

	D3D12_GPU_VIRTUAL_ADDRESS GetVertexGPUAddress();

	CBuffer BLAS_Scratch;
	CBuffer BLAS;

	void BuildBottomLevelAS(ID3D12GraphicsCommandList4* InCommandList);

	void CreateVertexShaderResourceView();
};