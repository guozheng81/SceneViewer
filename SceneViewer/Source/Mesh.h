#pragma once

#include "Utils.h"
#include "Renderer.h"

class CSceneObject;

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

	// Instance data storage - unowned pointers to scene objects
	std::vector<CSceneObject*> InstanceSceneObjects;

	bool bNeedsSceneObjectTransform = true;
	bool bNeedsAlphaTest = false;

	int GlobalInstanceIndex = 0;
	int AlbedoTextureIndex = 0;
	int NormalTextureIndex = -1;
	int PBRTextureIndex = -1;

	XMVECTOR BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
	XMVECTOR BoundingBoxMax = { 0.0f, 0.0f, 0.0f };

public:
	CMesh(const CMesh&) = delete;
	CMesh& operator=(const CMesh&) = delete;

	CMesh() = default;
	~CMesh() = default;

	// Allow move semantics
	CMesh(CMesh&&) noexcept = default;
	CMesh& operator=(CMesh&&) noexcept = default;

	CD3DX12_GPU_DESCRIPTOR_HANDLE VertexSrvGPUDescriptor = {};

	void Init(const std::vector<SSceneVertex>& Verts, const std::vector<UINT32>& Indices, int InTextureIdx = 0, int InNormalTextureIdx = -1, int InPBRTextureIdx = -1, bool bAlphaTest = false);
	void ResetUploadResource();
	void SetNeedsSceneObjectTransform(bool bInNeedsTransform) { bNeedsSceneObjectTransform = bInNeedsTransform; }

	void SetBoundingBox(const XMFLOAT3& InMin, const XMFLOAT3& InMax) 
	{
		BoundingBoxMin = XMLoadFloat3(&InMin);
		BoundingBoxMax = XMLoadFloat3(&InMax);
	}
	void GetBoundingBox(XMVECTOR& OutMin, XMVECTOR& OutMax) const { OutMin = BoundingBoxMin; OutMax = BoundingBoxMax; }

	// Instance management
	UINT AddInstance(CSceneObject* InSceneObject);
	void RemoveInstance(UINT InstanceIndex);
	void RemoveInstance(CSceneObject* InSceneObject);
	void GetInstanceWorldMatrix(UINT InstanceIndex, XMFLOAT4X4* OutMtx);
	CSceneObject* GetInstanceSceneObject(UINT InstanceIndex) const;
	void ClearInstances();
	UINT GetInstanceCount() const;
	int GetGlobalInstanceIndex() const { return GlobalInstanceIndex; }
	void SetGlobalInstanceIndex(int InIdx) { GlobalInstanceIndex = InIdx; }

	int GetAlbedoTextureIndex() const { return AlbedoTextureIndex; }
	int GetNormalTextureIndex() const { return NormalTextureIndex; }
	int GetPBRTextureIndex() const { return PBRTextureIndex; }

	void OnRender(ID3D12GraphicsCommandList* InCommandList);

	inline UINT GetVertexCount() const {	return VertexCount;	}
	inline UINT GetIndicesCount() const {	return IndicesCount;	}

	D3D12_GPU_VIRTUAL_ADDRESS GetVertexGPUAddress();

	CBuffer BLAS_Scratch;
	CBuffer BLAS;

	void BuildBottomLevelAS(ID3D12GraphicsCommandList4* InCommandList);

	void CreateVertexShaderResourceView();
};