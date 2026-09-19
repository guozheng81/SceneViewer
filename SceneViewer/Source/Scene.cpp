#include "Scene.h"
#include "Renderer.h"
#include "Mesh.h"
#include "Logger.h"
#include "SceneObject.h"
#include "json.hpp"
#include <fstream>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

using Json = nlohmann::json;

CScene::CScene()
{
	MainCamera.SetAspectRatio(CRenderer::GetInstance().ViewportWidth, CRenderer::GetInstance().ViewportHeight);
	MainCamera.SetFOV(55.0f);
	MainCamera.SetPositionAndRotation(XMFLOAT3(0.0f, 200.0f, 0.0f), XMConvertToRadians(-90.0f), 0.0f);

	Material = std::make_unique<CMaterial>();
	Material->PSODesc.NumRenderTargets = 2;
	Material->PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	Material->PSODesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	//Material->PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
}

void CScene::LoadObjFile(const std::filesystem::path& InObjPath, CSceneObject* InParentSceneObject)
{
	tinyobj::ObjReader* TinyObjReader = ObjReaderCache[InObjPath].get();
	if(TinyObjReader == nullptr)
	{
		std::unique_ptr<tinyobj::ObjReader> NewReader = std::make_unique<tinyobj::ObjReader>();
		TinyObjReader = NewReader.get();
		if (!TinyObjReader->ParseFromFile(InObjPath.string()))
		{
			LOG_WARN("Failed to load obj file: %s", InObjPath.string().c_str());
			return;
		}
		else
		{
			ObjReaderCache[InObjPath] = std::move(NewReader);
		}
	}

	std::vector<SSceneVertex> Verts;
	std::vector<UINT32>	Indices;

	auto& attrib = TinyObjReader->GetAttrib();
	auto& shapes = TinyObjReader->GetShapes();
	auto& materials = TinyObjReader->GetMaterials();

	for (size_t s = 0; s < shapes.size(); s++)
	{
		Verts.clear();
		Indices.clear();
		int CurrentMatIdx = shapes[s].mesh.material_ids[0];

		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{
			size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
			if (shapes[s].mesh.material_ids[f] != CurrentMatIdx)
			{
				auto TinyObjMat = materials[CurrentMatIdx];
				AddMesh(InParentSceneObject, Verts, Indices, TinyObjMat.diffuse_texname, TinyObjMat.bump_texname, TinyObjMat.roughness_texname);

				Verts.clear();
				Indices.clear();
				CurrentMatIdx = shapes[s].mesh.material_ids[f];
			}

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++)
			{
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
				tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
				tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

				tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
				tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
				tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

				tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
				tinyobj::real_t ty = 1.0f - attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

				//Indices.push_back((UINT)(Verts.size()));

				SSceneVertex Vert;
				Vert.Position = XMFLOAT3(vx, vy, vz);
				Vert.Normal = XMFLOAT3(nx, ny, nz);
				Vert.Tex = XMFLOAT2(tx, ty);
				Verts.push_back(Vert);
			}
			index_offset += fv;
		}

		auto TinyObjMat = materials[CurrentMatIdx];
		AddMesh(InParentSceneObject, Verts, Indices, TinyObjMat.diffuse_texname, TinyObjMat.bump_texname, TinyObjMat.roughness_texname);
	}
}

void	CScene::Init()
{
	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> SrvRanges;
	CMaterial::InitRootParameters(1, 1, 0, 1, RootParams, SrvRanges);

	CD3DX12_ROOT_PARAMETER MeshIdxRootParam;
	MeshIdxRootParam.InitAsConstants(1, 1);
	RootParams.push_back(MeshIdxRootParam);

	Material->BuildRootSignature(RootParams, false);
	Material->BuildPSO(L"Scene_VSMain.cso", L"Scene_PSMain.cso");

	CRenderer& RendererInst = CRenderer::GetInstance();
	GBufferA = RendererInst.CreateRenderTarget("GBufferA", DXGI_FORMAT_R8G8B8A8_UNORM, XMFLOAT4A(0.0f, 0.0f, 0.0f, 1.0f));
	GBufferB = RendererInst.CreateRenderTarget("GBufferB", DXGI_FORMAT_R8G8B8A8_UNORM, XMFLOAT4A(0.5f, 0.5f, 0.5f, 0.0f));
	Depth0 = RendererInst.CreateDepthTexture("Depth0", RendererInst.ViewportWidth, RendererInst.ViewportHeight);
	Depth1 = RendererInst.CreateDepthTexture("Depth1", RendererInst.ViewportWidth, RendererInst.ViewportHeight);

	SDescriptorHandle SrvDescriptorHandle = CRenderer::GetInstance().SrvUavDescriptorAllocator.Allocate();
	TLASGPUDescriptor = SrvDescriptorHandle.GpuHandle;
	TLASCPUDescriptor = SrvDescriptorHandle.CpuHandle;
}

void CScene::Load(const std::string& InSceneName, ID3D12GraphicsCommandList4* InCommandList)
{
	CRenderer& RendererInst = CRenderer::GetInstance();

	std::filesystem::path AssetPath = CRenderer::GetAssetDirectory();
	std::filesystem::path JsonPath = AssetPath / InSceneName;

	// all textures will be allocated in a single block, so we can use a single descriptor for all of them
	RendererInst.SrvUavDescriptorAllocator.BeginBlockAllocation(0);

	std::ifstream JsonFile(JsonPath);
	if (JsonFile)
	{
		Json SceneJson;
		try
		{
			JsonFile >> SceneJson;
		}
		catch (const Json::parse_error& Ex)
		{
			LOG_ERROR("Failed to parse scene json '%s': %s", JsonPath.string().c_str(), Ex.what());
			SceneJson = Json();
		}
		JsonFile.close();

		if (SceneJson.contains("Objects") && SceneJson["Objects"].is_array())
		{
			for (const auto& ObjectEntry : SceneJson["Objects"])
			{
				std::string ObjFileName = ObjectEntry.value("File", std::string());
				if (ObjFileName.empty())
				{
					continue;
				}

				CSceneObject* RootSceneObject = nullptr;

				int ExistingSceneObjectIndex = FindSceneObjectIndexByFileName(ObjFileName);
				if (ExistingSceneObjectIndex >= 0)
				{
					RootSceneObject = DuplicateSceneObject(ExistingSceneObjectIndex);
				}
				else
				{
					std::string ObjectName = ObjectEntry.value("Name", ObjFileName);
					RootSceneObject = CreateSceneObject(GetAvailableSceneObjectName(ObjectName));
					RootSceneObject->FileName = ObjFileName;
				}

				XMFLOAT3 Position(0.0f, 0.0f, 0.0f);
				if (ObjectEntry.contains("Position") && ObjectEntry["Position"].is_array() && ObjectEntry["Position"].size() >= 3)
				{
					const auto& PositionArray = ObjectEntry["Position"];
					Position = XMFLOAT3(PositionArray[0].get<float>(), PositionArray[1].get<float>(), PositionArray[2].get<float>());
				}
				RootSceneObject->SetPosition(Position);

				XMFLOAT3 Rotation(0.0f, 0.0f, 0.0f);
				if (ObjectEntry.contains("Rotation") && ObjectEntry["Rotation"].is_array() && ObjectEntry["Rotation"].size() >= 3)
				{
					const auto& RotationArray = ObjectEntry["Rotation"];
					Rotation = XMFLOAT3(RotationArray[0].get<float>(), RotationArray[1].get<float>(), RotationArray[2].get<float>());
				}
				RootSceneObject->SetRotation(Rotation);

				XMFLOAT3 Scale(1.0f, 1.0f, 1.0f);
				if (ObjectEntry.contains("Scale") && ObjectEntry["Scale"].is_array() && ObjectEntry["Scale"].size() >= 3)
				{
					const auto& ScaleArray = ObjectEntry["Scale"];
					Scale = XMFLOAT3(ScaleArray[0].get<float>(), ScaleArray[1].get<float>(), ScaleArray[2].get<float>());
				}
				RootSceneObject->SetScale(Scale);

				if (ExistingSceneObjectIndex < 0)
				{
					LoadObjFile(AssetPath / ObjFileName, RootSceneObject);
				}
			}
		}
	}
	else
	{
		LOG_ERROR("Failed to open scene json file: %s", JsonPath.string().c_str());
	}

	RendererInst.SrvUavDescriptorAllocator.EndBlockAllocation();

	CollectAllMeshesInfo();

	if (MaxModelElementCount == 0 && MeshInfoArray.size() != 0)
	{
		MaxModelElementCount = (UINT)(MeshInfoArray.size());
		ModelUploadBuffer.Init((UINT)(sizeof(SMeshInfo)), MaxModelElementCount, true, D3D12_RESOURCE_STATE_COMMON);
		ModelUploadBuffer.SetData(MeshInfoArray.data());

		ModelBuffer.Init((UINT)(sizeof(SMeshInfo)), MaxModelElementCount, false, D3D12_RESOURCE_STATE_COMMON);
		ModelBuffer.CreateShaderResourceView();

		TLAS_Instances.Init(sizeof(D3D12_RAYTRACING_INSTANCE_DESC), MaxModelElementCount, true);
	}

	BuildAccelerationStructures(InCommandList, true, true);
}

void CScene::Unload()
{
	AllSceneObjects.clear();
	AllMeshes.clear();
}

UINT CScene::CountAndCacheAllMeshes(const std::string& InSceneName)
{
	std::filesystem::path AssetPath = CRenderer::GetAssetDirectory();

	UINT TotalMeshCount = 0;
	std::ifstream JsonFile(AssetPath/ InSceneName);
	if (JsonFile)
	{
		Json SceneJson;
		try
		{
			JsonFile >> SceneJson;
		}
		catch (const Json::parse_error& Ex)
		{
			LOG_ERROR("Failed to parse scene json '%s': %s", AssetPath.string().c_str(), Ex.what());
			SceneJson = Json();
		}
		JsonFile.close();

		if (SceneJson.contains("Objects") && SceneJson["Objects"].is_array())
		{
			for (const auto& ObjectEntry : SceneJson["Objects"])
			{
				std::string ObjFileName = ObjectEntry.value("File", std::string());
				if (ObjFileName.empty())
				{
					continue;
				}

				TotalMeshCount += CountMeshInObjFileAndCache(AssetPath / ObjFileName);
			}
		}
	}

	return TotalMeshCount;
}

UINT CScene::CountMeshInObjFileAndCache(const std::filesystem::path& InPath)
{
	tinyobj::ObjReader* TinyObjReader = ObjReaderCache.find(InPath) != ObjReaderCache.end() ? ObjReaderCache[InPath].get() : nullptr;

	if(TinyObjReader != nullptr)
	{
		return 0;
	}
	else
	{
		std::unique_ptr<tinyobj::ObjReader> NewReader = std::make_unique<tinyobj::ObjReader>();
		TinyObjReader = NewReader.get();
		if (!TinyObjReader->ParseFromFile(InPath.string()))
		{
			LOG_WARN("Failed to load obj file: %s", InPath.string().c_str());
			return 0;
		}
		else
		{
			ObjReaderCache[InPath] = std::move(NewReader);
		}
	}

	auto& shapes = TinyObjReader->GetShapes();
	auto& materials = TinyObjReader->GetMaterials();

	UINT MeshCount = 0;
	for (const auto& Shape : shapes)
	{
		int PrevMatIdx = -2; // sentinel that never matches a real material id
		for (size_t f = 0; f < Shape.mesh.material_ids.size(); ++f)
		{
			int MatIdx = Shape.mesh.material_ids[f];
			if (MatIdx != PrevMatIdx)
			{
				++MeshCount;
				PrevMatIdx = MatIdx;
			}
		}
	}

	return MeshCount;	
}

void CScene::CalculateBoundingBox(std::vector<SSceneVertex>& Verts, XMFLOAT3& OutMin, XMFLOAT3& OutMax, XMFLOAT3& OutCenter, bool bRecenter)
{
	if (Verts.empty())
	{
		OutMin = XMFLOAT3(0.0f, 0.0f, 0.0f);
		OutMax = XMFLOAT3(0.0f, 0.0f, 0.0f);
		OutCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);
		return;
	}

	OutMin = Verts[0].Position;
	OutMax = Verts[0].Position;

	for (const auto& Vert : Verts)
	{
		OutMin.x = std::min(OutMin.x, Vert.Position.x);
		OutMin.y = std::min(OutMin.y, Vert.Position.y);
		OutMin.z = std::min(OutMin.z, Vert.Position.z);

		OutMax.x = std::max(OutMax.x, Vert.Position.x);
		OutMax.y = std::max(OutMax.y, Vert.Position.y);
		OutMax.z = std::max(OutMax.z, Vert.Position.z);
	}

	OutCenter.x = (OutMin.x + OutMax.x) * 0.5f;
	OutCenter.y = (OutMin.y + OutMax.y) * 0.5f;
	OutCenter.z = (OutMin.z + OutMax.z) * 0.5f;

	if (bRecenter)
	{
		for (auto& Vert : Verts)
		{
			Vert.Position.x -= OutCenter.x;
			Vert.Position.y -= OutCenter.y;
			Vert.Position.z -= OutCenter.z;
		}
	}
}

CMesh* CScene::AddMesh(CSceneObject* InSceneObject, std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices, const std::string& InDiffTexName, const std::string& InNormalTexName, const std::string& InPBRTexName)
{
	std::unique_ptr<CMesh> CurMesh = std::make_unique<CMesh>();

	std::string	NormalTextureName = InNormalTexName;

	int AlbedoTextureIdx = -1;
	if (!InDiffTexName.empty())
	{
		CTexture2D* DiffTexture = CRenderer::GetInstance().LoadTexture(InDiffTexName, true);
		if(DiffTexture)
		{
			AlbedoTextureIdx = CRenderer::GetInstance().GetSrvDescriptorOffset(CD3DX12_GPU_DESCRIPTOR_HANDLE(GetMaterialTexturesGPUDescriptor()), DiffTexture->SrvGPUDescriptor);
		}
	}

	int NormalTextureIdx = -1;
	if (!InNormalTexName.empty())
	{
		CTexture* NormalTexture = CRenderer::GetInstance().LoadTexture(NormalTextureName);
		if (NormalTexture)
		{
			NormalTextureIdx = CRenderer::GetInstance().GetSrvDescriptorOffset(CD3DX12_GPU_DESCRIPTOR_HANDLE(GetMaterialTexturesGPUDescriptor()), NormalTexture->SrvGPUDescriptor);
		}
	}

	int PBRTextureIdx = -1;
	if (!InPBRTexName.empty())
	{
		CTexture* PBRTexture = CRenderer::GetInstance().LoadTexture(InPBRTexName);
		if (PBRTexture)
		{
			PBRTextureIdx = CRenderer::GetInstance().GetSrvDescriptorOffset(CD3DX12_GPU_DESCRIPTOR_HANDLE(GetMaterialTexturesGPUDescriptor()), PBRTexture->SrvGPUDescriptor);
		}
	}

	bool bAlphaTest = (InDiffTexName.find("vase_plant") != std::string::npos || InDiffTexName.find("sponza_thorn") != std::string::npos || InDiffTexName.find("chain") != std::string::npos);

	bool bAddNewSceneObject = (InDiffTexName.find("vase_base") != std::string::npos);
	if (bAddNewSceneObject)
	{
		XMFLOAT3 Min, Max, Center;
		CalculateBoundingBox(Verts, Min, Max, Center, true);
		CurMesh->Init(Verts, Indices, AlbedoTextureIdx, NormalTextureIdx, PBRTextureIdx, bAlphaTest);

		Min.x -= Center.x;
		Min.y -= Center.y;
		Min.z -= Center.z;
		Max.x -= Center.x;
		Max.y -= Center.y;
		Max.z -= Center.z;
		CurMesh->SetBoundingBox(Min, Max);

		// allow it to have its own transform, so we can move it around
		std::string NewSceneObjectName = GetAvailableSceneObjectName(InDiffTexName);
		CSceneObject* NewSceneObject = CreateSceneObject(NewSceneObjectName);
		InSceneObject->AddChild(NewSceneObject);
		NewSceneObject->SetPosition(Center);
		NewSceneObject->AddMesh(CurMesh.get());
	}
	else
	{
		CurMesh->Init(Verts, Indices, AlbedoTextureIdx, NormalTextureIdx, PBRTextureIdx, bAlphaTest);
		XMFLOAT3 Min, Max, Center;
		CalculateBoundingBox(Verts, Min, Max, Center, false);
		CurMesh->SetBoundingBox(Min, Max);

		InSceneObject->AddMesh(CurMesh.get());
	}

	CMesh* Res = CurMesh.get();
	AllMeshes.push_back(std::move(CurMesh));
	return Res;
}

std::string CScene::GetAvailableSceneObjectName(const std::string& InBaseName)
{
	std::string ResultName = InBaseName;
	size_t DelimiterPos = InBaseName.find_first_of("_.");
	if (DelimiterPos != std::string::npos)
	{
		ResultName = InBaseName.substr(0, DelimiterPos);
	}

	// Check if name is already used in AllSceneObjects
	auto IsNameUsed = [this](const std::string& NameToCheck) -> bool
		{
			for (const auto& SceneObj : AllSceneObjects)
			{
				if (SceneObj && SceneObj->Name == NameToCheck)
				{
					return true;
				}
			}
			return false;
		};

	if (!IsNameUsed(ResultName))
	{
		return ResultName;
	}

	// Name is used, try appending/incrementing numeric suffix
	int SuffixNumber = 0;

	// Check if ResultName already ends with a number
	if (!ResultName.empty() && std::isdigit(ResultName.back()))
	{
		size_t NumberStartIdx = ResultName.length() - 1;
		while (NumberStartIdx > 0 && std::isdigit(ResultName[NumberStartIdx - 1]))
		{
			--NumberStartIdx;
		}
		SuffixNumber = std::stoi(ResultName.substr(NumberStartIdx));
		ResultName = ResultName.substr(0, NumberStartIdx);
	}

	// Increment and find available name
	std::string CandidateName;
	do
	{
		CandidateName = ResultName + std::to_string(SuffixNumber);
		++SuffixNumber;
	} while (IsNameUsed(CandidateName));

	return CandidateName;
}

void CScene::CollectAllMeshesInfo()
{
	size_t PreMeshCount = MeshInfoArray.size();

	MeshInfoArray.clear();
	int MeshIdx = 0;
	for (auto& CurMesh : AllMeshes)
	{
		CurMesh->SetGlobalInstanceIndex((int)MeshInfoArray.size());
		for (int InstanceIdx = 0; InstanceIdx < CurMesh->GetInstanceCount(); ++InstanceIdx)
		{
			SMeshInfo MeshInfo;
			MeshInfo.MeshIdx = MeshIdx;
			MeshInfo.AlbedoTextureIdx = CurMesh->GetAlbedoTextureIndex();
			MeshInfo.NormalTextureIdx = CurMesh->GetNormalTextureIndex();
			MeshInfo.PBRTextureIdx = CurMesh->GetPBRTextureIndex();
			CurMesh->GetInstanceWorldMatrix(InstanceIdx, &(MeshInfo.WorldMatrix));
			CSceneObject* SceneObj = CurMesh->GetInstanceSceneObject(InstanceIdx);
			if (SceneObj)
			{
				MeshInfo.Albedo = SceneObj->Albedo;
				MeshInfo.Roughness = SceneObj->Roughness;
				MeshInfo.Metallic = SceneObj->Metallic;
			}
			MeshInfoArray.push_back(MeshInfo);
		}

		MeshIdx++;
	}

	bNeedRebuildTLAS = (PreMeshCount != MeshInfoArray.size());
	bIsModelBufferDirty = true;

	GetSceneBoundingBox(BoundingBoxMin, BoundingBoxMax);
}

CMaterial* CScene::GetSceneMaterial()
{
	return Material.get();
}

CScene::~CScene()
{
}

void CScene::OnLoaded()
{
	CRenderer::GetInstance().SrvUavDescriptorAllocator.BeginBlockAllocation(1);
	for (auto& CurMesh : AllMeshes)
	{
		CurMesh->ResetUploadResource();
		// for raytracing
		CurMesh->CreateVertexShaderResourceView();
	}
	CRenderer::GetInstance().SrvUavDescriptorAllocator.EndBlockAllocation();

	ObjReaderCache.clear();
}

void	CScene::SetDirectionalLight(const XMFLOAT3& InDir, float Intensity)
{
	XMVECTOR LightDirV = XMLoadFloat3(&InDir);
	DirectionalLightDir = XMVector3Normalize(LightDirV);
	DirectionalLightIntensity = Intensity;
}

void CScene::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
	bIsUsingDepth0 = (!bIsUsingDepth0);

	if(bIsModelBufferDirty)
	{
		if (bNeedRebuildTLAS)
		{
			CRenderer::GetInstance().FlushCommandQueue();
		}

		UINT CurModelEleCount = (UINT)(MeshInfoArray.size());
		if(MaxModelElementCount < CurModelEleCount)
		{
			MaxModelElementCount = (UINT)(CurModelEleCount *1.5f);
			ModelBuffer.ResizeElementCount(MaxModelElementCount);
			ModelUploadBuffer.ResizeElementCount(MaxModelElementCount);

			TLAS_Instances.ResizeElementCount(MaxModelElementCount);
		}

		if(CurModelEleCount > 0)
		{
			// copy modelUploadBuffer to modelBuffer
			CRenderer::GetInstance().ResourceBarrier(ModelBuffer.GetResource(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

			ModelUploadBuffer.SetData(MeshInfoArray.data(), CurModelEleCount);
			UINT BufferSize = (UINT)(sizeof(SMeshInfo)) * CurModelEleCount;
			InCommandList->CopyBufferRegion(ModelBuffer.GetResource(), 0, ModelUploadBuffer.GetResource(), 0, BufferSize);

			CRenderer::GetInstance().ResourceBarrier(ModelBuffer.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}

		BuildAccelerationStructures(InCommandList, false, bNeedRebuildTLAS);

		bIsModelBufferDirty = false;
	}

	CRenderer::GetInstance().ResourceBarrier(GBufferA->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	CRenderer::GetInstance().ResourceBarrier(GBufferB->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	CRenderer::GetInstance().ResourceBarrier(GetDepthTexture()->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	Material->OnRender(InCommandList);

	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandles[2] = { GBufferA->RtvCPUDescriptor, GBufferB->RtvCPUDescriptor };
	CD3DX12_CPU_DESCRIPTOR_HANDLE DsvHandle = GetDepthTexture()->DsvCPUDescriptor;
	InCommandList->OMSetRenderTargets(2, RtvHandles, true, &DsvHandle);

	float ClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	InCommandList->ClearRenderTargetView(RtvHandles[0], ClearColor, 0, nullptr);

	float ClearColorB[] = { 0.5f, 0.5f, 0.5f, 0.0f };
	InCommandList->ClearRenderTargetView(RtvHandles[1], ClearColorB, 0, nullptr);

	InCommandList->ClearDepthStencilView(DsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	Material->SetConstantBuffer(InCommandList, 0, CRenderer::GetInstance().GetCurrentViewBuffer());

	if(MaxModelElementCount > 0)
	{
		Material->SetShaderResource(InCommandList, 0, &ModelBuffer);
	}

	int TexturesParam = Material->FindSrvRootParameterIndex(0, 1);
	if (TexturesParam >= 0)
	{
		InCommandList->SetGraphicsRootDescriptorTable(TexturesParam, GetMaterialTexturesGPUDescriptor());
	}
	
	int MeshIndexParam = Material->FindConstantRootParameterIndex(1);
	if (MeshIndexParam < 0)
	{
		return;
	}
	
	for(int i = 0; i < AllMeshes.size(); ++i)
	{
		auto& CurMesh = AllMeshes[i];
		InCommandList->SetGraphicsRoot32BitConstant(MeshIndexParam, CurMesh->GetGlobalInstanceIndex(), 0);
		CurMesh->OnRender(InCommandList);
	}

	CRenderer::GetInstance().ResourceBarrier(GBufferB->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	CRenderer::GetInstance().ResourceBarrier(GetDepthTexture()->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void CScene::BuildAccelerationStructures(ID3D12GraphicsCommandList4* InCommandList, bool bBuildBLAS, bool bFullRebuild)
{
	UINT MeshNum = AllMeshes.size();

	if(bBuildBLAS)
	{
		for (UINT i = 0; i < MeshNum; ++i)
		{
			auto& CurMesh = AllMeshes[i];
			CurMesh->BuildBottomLevelAS(InCommandList);
		}
	}

	UINT InstanceNum = MeshInfoArray.size();

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Inputs = {};
	Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	Inputs.NumDescs = InstanceNum;
	Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO Info;
	CRenderer::GetInstance().D3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs, &Info);

	if (bFullRebuild)
	{
		TLAS_Scratch.Reset();
		TLAS.Reset();

		TLAS_Scratch.Init(Info.ScratchDataSizeInBytes, 1, false, D3D12_RESOURCE_STATE_COMMON, true);
		TLAS.Init(Info.ResultDataMaxSizeInBytes, 1, false, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
	}
	else
	{
		CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(TLAS.GetResource());
		InCommandList->ResourceBarrier(1, &UavBarrier);
	}

	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> InstancesDescArray(InstanceNum);
	UINT InstanceIdx = 0;
	for (UINT i = 0; i < MeshNum; ++i)
	{
		auto& CurMesh = AllMeshes[i];

		for(UINT j = 0; j < CurMesh->GetInstanceCount(); ++j)
		{
			SMeshInfo& MeshInfo = MeshInfoArray[CurMesh->GetGlobalInstanceIndex() + j];
			XMFLOAT4X4 Mtx = MeshInfo.WorldMatrix;
			memcpy(InstancesDescArray[InstanceIdx].Transform, &Mtx, sizeof(InstancesDescArray[InstanceIdx].Transform));

			InstancesDescArray[InstanceIdx].InstanceID = InstanceIdx;
			InstancesDescArray[InstanceIdx].InstanceContributionToHitGroupIndex = 0;
			InstancesDescArray[InstanceIdx].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			InstancesDescArray[InstanceIdx].AccelerationStructure = CurMesh->BLAS.GetGPUAddress();
			InstancesDescArray[InstanceIdx].InstanceMask = 0xFF;

			InstanceIdx++;
		}
	}
	TLAS_Instances.SetData(InstancesDescArray.data(), InstanceNum);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC AsDesc = {};
	AsDesc.Inputs = Inputs;
	AsDesc.Inputs.InstanceDescs = TLAS_Instances.GetGPUAddress();
	AsDesc.DestAccelerationStructureData = TLAS.GetGPUAddress();
	AsDesc.ScratchAccelerationStructureData = TLAS_Scratch.GetGPUAddress();

	if(!bFullRebuild)
	{
		AsDesc.SourceAccelerationStructureData = TLAS.GetGPUAddress();
		AsDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	}

	InCommandList->BuildRaytracingAccelerationStructure(&AsDesc, 0, nullptr);

	CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(TLAS.GetResource());
	InCommandList->ResourceBarrier(1, &UavBarrier);

	if(bFullRebuild)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC TLASSrvDesc = {};
		TLASSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		TLASSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		TLASSrvDesc.RaytracingAccelerationStructure.Location = TLAS.GetGPUAddress();

		CRenderer::GetInstance().D3dDevice->CreateShaderResourceView(nullptr, &TLASSrvDesc, TLASCPUDescriptor);
	}
}

CSceneObject* CScene::CreateSceneObject(std::string InName)
{
	auto SceneObj = std::make_unique<CSceneObject>(std::move(InName));
	CSceneObject* Ptr = SceneObj.get();

	AllSceneObjects.push_back(std::move(SceneObj));
	return Ptr;
}

void CScene::CollectSceneObjectSubtree(CSceneObject* InSceneObject, std::vector<CSceneObject*>& OutSubtree)
{
	if (!InSceneObject)
	{
		return;
	}

	OutSubtree.push_back(InSceneObject);
	for (CSceneObject* Child : InSceneObject->GetChildren())
	{
		CollectSceneObjectSubtree(Child, OutSubtree);
	}
}

void CScene::DeleteSceneObject(int InIndex)
{
	if (InIndex >= AllSceneObjects.size())
	{
		LOG_ERROR("CScene::DeleteSceneObject: Index %zu out of range.", InIndex);
		return;
	}

	CSceneObject* TargetObject = AllSceneObjects[InIndex].get();

	if (TargetObject->GetParent())
	{
		TargetObject->GetParent()->RemoveChild(TargetObject);
	}

	// Gather the target and all descendants, since deleting a node deletes its subtree
	std::vector<CSceneObject*> ObjectsToDelete;
	CollectSceneObjectSubtree(TargetObject, ObjectsToDelete);

	AllSceneObjects.erase(
		std::remove_if(AllSceneObjects.begin(), AllSceneObjects.end(),
			[&ObjectsToDelete](const std::unique_ptr<CSceneObject>& Candidate)
			{
				return std::find(ObjectsToDelete.begin(), ObjectsToDelete.end(), Candidate.get()) != ObjectsToDelete.end();
			}),
		AllSceneObjects.end());
}

CSceneObject* CScene::DuplicateSceneObjectRecursive(CSceneObject* InSceneObject, CSceneObject* InNewParent)
{
	if (!InSceneObject)
	{
		return nullptr;
	}

	std::string NewName = GetAvailableSceneObjectName(InSceneObject->Name);
	CSceneObject* NewSceneObject = CreateSceneObject(NewName);
	NewSceneObject->FileName = InSceneObject->FileName;

	NewSceneObject->SetPosition(InSceneObject->GetLocalPosition());
	NewSceneObject->SetRotation(InSceneObject->GetLocalRotation());
	NewSceneObject->SetScale(InSceneObject->GetLocalScale());

	for (CMesh* Mesh : InSceneObject->GetMeshes())
	{
		NewSceneObject->AddMesh(Mesh);
	}

	if (InNewParent)
	{
		InNewParent->AddChild(NewSceneObject);
	}

	for (CSceneObject* Child : InSceneObject->GetChildren())
	{
		DuplicateSceneObjectRecursive(Child, NewSceneObject);
	}

	return NewSceneObject;
}

CSceneObject* CScene::DuplicateSceneObject(int InIndex)
{
	if (InIndex >= AllSceneObjects.size())
	{
		LOG_ERROR("CScene::DuplicateSceneObject: Index %zu out of range.", InIndex);
		return nullptr;
	}

	CSceneObject* SourceObject = AllSceneObjects[InIndex].get();

	// Note: DuplicateSceneObjectRecursive calls CreateSceneObject, which appends to AllSceneObjects.
	// Since SourceObject is a raw pointer (owned separately by unique_ptr), this remains valid across
	// vector growth/reallocation.
	CSceneObject* NewRoot = DuplicateSceneObjectRecursive(SourceObject, SourceObject->GetParent());

	return NewRoot;
}

int CScene::FindSceneObjectIndex(CSceneObject* InSceneObject) const
{
    for (size_t i = 0; i < AllSceneObjects.size(); ++i)
    {
        if (AllSceneObjects[i].get() == InSceneObject)
        {
            return (int)i;
        }
    }
    return -1;
}

int CScene::FindSceneObjectIndexByFileName(const std::string& InFileName) const
{
	for (int i = 0; i < AllSceneObjects.size(); ++i)
	{
		if (AllSceneObjects[i]->FileName == InFileName)
		{
			return i;
		}
	}
	return -1;
}

D3D12_GPU_DESCRIPTOR_HANDLE CScene::GetMaterialTexturesGPUDescriptor() const
{
	return CRenderer::GetInstance().SrvUavDescriptorAllocator.GetReservedBlockGpuHandle(0);
}

D3D12_GPU_DESCRIPTOR_HANDLE CScene::GetVertexBuffersGPUDescriptor() const
{
	return CRenderer::GetInstance().SrvUavDescriptorAllocator.GetReservedBlockGpuHandle(1);
}

void CScene::GetSceneBoundingBox(XMFLOAT3& OutMin, XMFLOAT3& OutMax)
{
	XMVECTOR SceneMin = XMVectorZero();
	XMVECTOR SceneMax = XMVectorZero();

	bool bHasAnyPoint = false;

	// iterate through all scene objects to find the overall bounding box
	for (auto& CurSceneObject : AllSceneObjects)
	{
		XMVECTOR ObjectMin, ObjectMax;
		CurSceneObject->GetBoundingBox(ObjectMin, ObjectMax);

		if (!bHasAnyPoint)
		{
			SceneMin = ObjectMin;
			SceneMax = ObjectMax;
			bHasAnyPoint = true;
		}
		else
		{
			SceneMin = XMVectorMin(SceneMin, ObjectMin);
			SceneMax = XMVectorMax(SceneMax, ObjectMax);
		}
	}

	XMStoreFloat3(&OutMin, SceneMin);
	XMStoreFloat3(&OutMax, SceneMax);
}