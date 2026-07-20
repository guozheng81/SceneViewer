#pragma once

#include "Utils.h"
#include "Material.h"
#include "Camera.h"
#include "Renderer.h"

class CMesh;

class CScene
{
protected:
	CCamera MainCamera;
	std::vector<std::unique_ptr<CMesh>> AllMeshes;
	std::unique_ptr<CMaterial>	Material;

	std::vector<SMeshInfo> MeshInfoArray;

	CBuffer ModelBuffer;

	CTexture2D* GBufferA = nullptr;
	CTexture2D* GBufferB = nullptr;
	CTexture2D* Depth0 = nullptr;
	CTexture2D* Depth1 = nullptr;
	bool bIsUsingDepth0 = false;

	CBuffer TLAS_Scratch;
	CBuffer TLAS;
	CBuffer TLAS_Instances;

	void BuildAccelerationStructures(ID3D12GraphicsCommandList4* InCommandList);

public:
	XMVECTOR DirectionalLightDir;
	float	 DirectionalLightIntensity = 4.0f;

	CD3DX12_GPU_DESCRIPTOR_HANDLE MaterialTexturesDescriptor = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE VertexBuffersDescriptor = {};
	D3D12_GPU_DESCRIPTOR_HANDLE TLASGPUDescriptor = {};

	CScene();
	~CScene();

	void	Load(const std::string& InSceneName, ID3D12GraphicsCommandList4* InCommandList);

	CMesh* AddMesh(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices, const std::string& InDiffTexName, const std::string& InNormalTexName);
	CMaterial* GetSceneMaterial();

	void	SetDirectionalLight(const XMFLOAT3& InDir, float Intensity);

	CCamera* GetMainCamera()
	{
		return &MainCamera;
	}

	CBuffer* GetModelBuffer() {	return &ModelBuffer;	}

	inline CTexture2D* GetDepthTexture() {
		return (bIsUsingDepth0 ? Depth0 : Depth1);
	}

	inline CTexture2D* GetHistoryDepthTexture() {
		return (bIsUsingDepth0 ? Depth1 : Depth0);
	}

	void OnRender(ID3D12GraphicsCommandList4* InCommandList);

	void OnLoaded();
};

