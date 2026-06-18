#pragma once

#include "Utils.h"
#include "Material.h"
#include "Camera.h"

class CMesh
{
protected:
	UINT	IndicesCount = 0;

public:
	ComPtr<ID3D12Resource> VertexBuffer;
	ComPtr<ID3D12Resource> IndexBuffer;

	ComPtr<ID3D12Resource> VertexUploadBuffer;
	ComPtr<ID3D12Resource> IndexUploadBuffer;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW	 IndexBufferView;

	void Init(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices);

	void OnRender(ID3D12GraphicsCommandList* InCommandList);
};

class CScene
{
protected:
	CCamera MainCamera;
	std::vector<std::unique_ptr<CMesh>> AllMeshes;
	std::unique_ptr<CMaterial>	Material;

public:

	CScene();
	~CScene();

	void	Load();

	CMaterial* GetSceneMaterial();

	CCamera* GetMainCamera()
	{
		return &MainCamera;
	}

	void OnRender(ID3D12GraphicsCommandList* InCommandList);
};

