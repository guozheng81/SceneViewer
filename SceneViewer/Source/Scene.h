#pragma once

#include "Utils.h"
#include "Material.h"
#include "Camera.h"
#include "Renderer.h"

namespace tinyobj
{
	class ObjReader;
}

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

	UINT MaxModelElementCount = 0;
	CBuffer ModelUploadBuffer;
	CBuffer ModelBuffer;
	bool bIsModelBufferDirty = false;
	bool bNeedRebuildTLAS = false;

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

	void BuildAccelerationStructures(ID3D12GraphicsCommandList4* InCommandList, bool bBuildBLAS, bool bFullRebuild);

	void CollectSceneObjectSubtree(CSceneObject* InSceneObject, std::vector<CSceneObject*>& OutSubtree);
	CSceneObject* DuplicateSceneObjectRecursive(CSceneObject* InSceneObject, CSceneObject* InNewParent);

	std::map<std::filesystem::path, std::unique_ptr<tinyobj::ObjReader>> ObjReaderCache;
	void LoadObjFile(const std::filesystem::path& InObjPath, CSceneObject* InParentSceneObject);

	UINT CountMeshInObjFileAndCache(const std::filesystem::path& InPath);

public:
	XMVECTOR DirectionalLightDir;
	float	 DirectionalLightIntensity = 4.0f;

	D3D12_GPU_DESCRIPTOR_HANDLE GetMaterialTexturesGPUDescriptor() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetVertexBuffersGPUDescriptor() const;

	D3D12_GPU_DESCRIPTOR_HANDLE TLASGPUDescriptor = {};
	D3D12_CPU_DESCRIPTOR_HANDLE TLASCPUDescriptor = {};

	CScene();
	~CScene();

	void	Init();
	void	Load(const std::string& InSceneName, ID3D12GraphicsCommandList4* InCommandList);
	void Unload();

	CMesh* AddMesh(CSceneObject* InSceneObject, std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices, const std::string& InDiffTexName, const std::string& InNormalTexName, const std::string& InPBRTexName);
	CMaterial* GetSceneMaterial();

	void	SetDirectionalLight(const XMFLOAT3& InDir, float Intensity);

	CCamera* GetMainCamera()
	{
		return &MainCamera;
	}

	CBuffer* GetModelBuffer() {	return MaxModelElementCount == 0? nullptr : &ModelBuffer;	}

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

	void DeleteSceneObject(int InIndex);
	CSceneObject* DuplicateSceneObject(int InIndex);

	int FindSceneObjectIndex(CSceneObject* InSceneObject) const;
	int FindSceneObjectIndexByFileName(const std::string& InFileName) const;
	UINT CountAndCacheAllMeshes(const std::string& InSceneName);
};

