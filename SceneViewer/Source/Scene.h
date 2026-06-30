#pragma once

#include "Utils.h"
#include "Material.h"
#include "Camera.h"
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

	XMMATRIX WorldMatrix = XMMatrixIdentity();

public:

	void Init(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices);
	void ResetUploadResource();

	void	GetWorldMatrix(XMFLOAT4X4* OutMtx);

	void OnRender(ID3D12GraphicsCommandList* InCommandList);
};

class CScene
{
protected:
	CCamera MainCamera;
	std::vector<std::unique_ptr<CMesh>> AllMeshes;
	std::unique_ptr<CMaterial>	Material;

	std::vector<SMeshInfo> MeshInfoArray;

	CUniformBuffer ModelBuffer;

	CD3DX12_GPU_DESCRIPTOR_HANDLE MaterialTexturesStartDspt = {};

public:
	XMVECTOR DirectionalLightDir;
	float	 DirectionalLightIntensity = 4.0f;

	CScene();
	~CScene();

	void	Load();

	CMesh* AddMesh(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices, const std::string& InDiffTexName, const std::string& InNormalTexName);
	CMaterial* GetSceneMaterial();

	void	SetDirectionalLight(const XMFLOAT3& InDir, float Intensity);

	CCamera* GetMainCamera()
	{
		return &MainCamera;
	}

	void OnRender(ID3D12GraphicsCommandList* InCommandList);

	void OnLoaded();
};

