#pragma once

#include "Utils.h"
#include "Material.h"

class CMesh
{
public:
	ComPtr<ID3D12Resource> VertexBuffer;
	ComPtr<ID3D12Resource> IndexBuffer;

	ComPtr<ID3D12Resource> VertexUploadBuffer;
	ComPtr<ID3D12Resource> IndexUploadBuffer;

	void OnRender() {}
};

class CScene
{
public:
	std::shared_ptr<CMaterial>	Material;
	std::vector<std::shared_ptr<CMesh>> AllMeshes;

	CScene();
	~CScene();

	void OnRender();
};

