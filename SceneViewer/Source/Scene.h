#pragma once

#include "Utils.h"
#include "Material.h"
#include "Camera.h"
#include "Renderer.h"

class CSceneObject;
class CMesh;

class CScene
{
protected:
	CCamera MainCamera;
	std::vector<std::unique_ptr<CMesh>> AllMeshes;
	std::unique_ptr<CMaterial>	Material;

	std::vector<SMeshInfo> MeshInfoArray;

	std::vector<std::unique_ptr<CSceneObject>> AllSceneObjects;

	CBuffer ModelUploadBuffer;
	CBuffer ModelBuffer;
	bool bIsModelBufferDirty = false;

	CTextureRenderTarget* GBufferA = nullptr;
	CTextureRenderTarget* GBufferB = nullptr;
	CTextureDepthStencil* Depth0 = nullptr;
	CTextureDepthStencil* Depth1 = nullptr;
	bool bIsUsingDepth0 = false;

	CBuffer TLAS_Scratch;
	CBuffer TLAS;
	CBuffer TLAS_Instances;

	void CalculateBoundingBox(std::vector<SSceneVertex>& Verts, XMFLOAT3& OutMin, XMFLOAT3& OutMax, XMFLOAT3& OutCenter, bool bRecenter);
	std::string GetAvailableSceneObjectName(const std::string& InBaseName);

	void BuildAccelerationStructures(ID3D12GraphicsCommandList4* InCommandList, bool bIsInit);

public:
	XMVECTOR DirectionalLightDir;
	float	 DirectionalLightIntensity = 4.0f;

	CD3DX12_GPU_DESCRIPTOR_HANDLE MaterialTexturesDescriptor = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE VertexBuffersDescriptor = {};
	D3D12_GPU_DESCRIPTOR_HANDLE TLASGPUDescriptor = {};

	CScene();
	~CScene();

	void	Load(const std::string& InSceneName, ID3D12GraphicsCommandList4* InCommandList);

	CMesh* AddMesh(CSceneObject* InSceneObject, std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices, const std::string& InDiffTexName, const std::string& InNormalTexName);
	CMaterial* GetSceneMaterial();

	void	SetDirectionalLight(const XMFLOAT3& InDir, float Intensity);

	CCamera* GetMainCamera()
	{
		return &MainCamera;
	}

	CBuffer* GetModelBuffer() {	return &ModelBuffer;	}

	inline CTextureDepthStencil* GetDepthTexture() {
		return (bIsUsingDepth0 ? Depth0 : Depth1);
	}

	inline CTextureDepthStencil* GetHistoryDepthTexture() {
		return (bIsUsingDepth0 ? Depth1 : Depth0);
	}

	void OnRender(ID3D12GraphicsCommandList4* InCommandList);

	void OnLoaded();

	void CollectAllMeshesInfo();

	CSceneObject* CreateSceneObject(std::string InName);
	inline const std::vector<std::unique_ptr<CSceneObject>>& GetAllSceneObjects() const
	{
		return AllSceneObjects;
	}
};

