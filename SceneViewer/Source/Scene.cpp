#include "Scene.h"
#include "Renderer.h"
#include "Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"


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

void CScene::Load(const std::string& InSceneName, ID3D12GraphicsCommandList4* InCommandList)
{
	std::vector<CD3DX12_ROOT_PARAMETER>	RootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE> SrvRanges;
	CMaterial::IntRootParameters(1, 1, 0, 1, RootParams, SrvRanges);

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

	std::filesystem::path AssetPath = CRenderer::GetAssetDirectory();
	AssetPath /= InSceneName;

	tinyobj::ObjReaderConfig ReaderConfig;
	ReaderConfig.mtl_search_path = "";

	tinyobj::ObjReader TinyObjReader;

	std::vector<SSceneVertex> Verts;
	std::vector<UINT32>	Indices;

	// all textures will be allocated in a single block, so we can use a single descriptor for all of them
	MaterialTexturesDescriptor = RendererInst.SrvUavDescriptorAllocator.BeginBlockAllocation();

	if (TinyObjReader.ParseFromFile(AssetPath.string(), ReaderConfig))
	{
		auto& attrib = TinyObjReader.GetAttrib();
		auto& shapes = TinyObjReader.GetShapes();
		auto& materials = TinyObjReader.GetMaterials();

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
					AddMesh(Verts, Indices, TinyObjMat.diffuse_texname, TinyObjMat.bump_texname);

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
			AddMesh(Verts, Indices, TinyObjMat.diffuse_texname, TinyObjMat.bump_texname);
		}
	}

	RendererInst.SrvUavDescriptorAllocator.EndBlockAllocation();

	BuildAccelerationStructures(InCommandList);
}

CMesh* CScene::AddMesh(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices, const std::string& InDiffTexName, const std::string& InNormalTexName)
{
	std::unique_ptr<CMesh> CurMesh = std::make_unique<CMesh>();

	std::string	NormalTextureName = InNormalTexName;
	if (InNormalTexName.empty())
	{
		NormalTextureName = std::string("default_normal_") + InDiffTexName;
	}

	CTexture2D* DiffTexture = CRenderer::GetInstance().LoadTexture(InDiffTexName, true);
	CRenderer::GetInstance().LoadTexture(NormalTextureName);

	bool bAlphaTest = (InDiffTexName.find("vase_plant") != std::string::npos || InDiffTexName.find("sponza_thorn") != std::string::npos || InDiffTexName.find("chain") != std::string::npos);

	CurMesh->Init(Verts, Indices, bAlphaTest);

	int TextureIdx = CRenderer::GetInstance().GetSrvDescriptorOffset(MaterialTexturesDescriptor, DiffTexture->SrvGPUDescriptor);
	SMeshInfo MeshInfo;
	MeshInfo.TextureIdx = TextureIdx / 2;
	CurMesh->GetWorldMatrix(&(MeshInfo.WorldMatrix));
	MeshInfoArray.push_back(MeshInfo);

	CMesh* Res = CurMesh.get();
	AllMeshes.push_back(std::move(CurMesh));
	return Res;
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
	VertexBuffersDescriptor = CRenderer::GetInstance().SrvUavDescriptorAllocator.BeginBlockAllocation();
	for (auto& CurMesh : AllMeshes)
	{
		CurMesh->ResetUploadResource();
		// for raytracing
		CurMesh->CreateVertexShaderResourceView();
	}
	CRenderer::GetInstance().SrvUavDescriptorAllocator.EndBlockAllocation();

	ModelBuffer.Init((UINT)(sizeof(SMeshInfo)), (UINT)(AllMeshes.size()), true);
	ModelBuffer.SetData(MeshInfoArray.data());
	ModelBuffer.CreateShaderResourceView();

	TLAS_Scratch.Reset();

	D3D12_SHADER_RESOURCE_VIEW_DESC TLASSrvDesc = {};
	TLASSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	TLASSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	TLASSrvDesc.RaytracingAccelerationStructure.Location = TLAS.GetGPUAddress();

	SDescriptorHandle SrvDescriptorHandle = CRenderer::GetInstance().SrvUavDescriptorAllocator.Allocate();
	TLASGPUDescriptor = SrvDescriptorHandle.GpuHandle;

	CRenderer::GetInstance().D3dDevice->CreateShaderResourceView(nullptr, &TLASSrvDesc, SrvDescriptorHandle.CpuHandle);
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

	Material->SetShaderResource(InCommandList, 0, &ModelBuffer);

	int TexturesParam = Material->FindSrvRootParameterIndex(0, 1);
	if (TexturesParam >= 0)
	{
		InCommandList->SetGraphicsRootDescriptorTable(TexturesParam, MaterialTexturesDescriptor);
	}
	
	int MeshIndexParam = Material->FindConstantRootParameterIndex(1);
	if (MeshIndexParam < 0)
	{
		return;
	}
	
	for(int i = 0; i < AllMeshes.size(); ++i)
	{
		auto& CurMesh = AllMeshes[i];
		InCommandList->SetGraphicsRoot32BitConstant(MeshIndexParam, i, 0);
		CurMesh->OnRender(InCommandList);
	}

	CRenderer::GetInstance().ResourceBarrier(GBufferB->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	CRenderer::GetInstance().ResourceBarrier(GetDepthTexture()->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void CScene::BuildAccelerationStructures(ID3D12GraphicsCommandList4* InCommandList)
{
	UINT MeshNum = AllMeshes.size();
	if (MeshNum == 0)
	{
		return;
	}

	for (UINT i = 0; i < MeshNum; ++i)
	{
		auto& CurMesh = AllMeshes[i];
		CurMesh->BuildBottomLevelAS(InCommandList);
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Inputs = {};
	Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	Inputs.NumDescs = MeshNum;
	Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO Info;
	CRenderer::GetInstance().D3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs, &Info);

	TLAS_Scratch.Init(Info.ScratchDataSizeInBytes, 1, false, D3D12_RESOURCE_STATE_COMMON, true);
	TLAS.Init(Info.ResultDataMaxSizeInBytes, 1, false, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
	TLAS_Instances.Init(sizeof(D3D12_RAYTRACING_INSTANCE_DESC), MeshNum, true);

	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> InstancesDescArray(MeshNum);
	for (UINT i = 0; i < MeshNum; ++i)
	{
		auto& CurMesh = AllMeshes[i];

		InstancesDescArray[i].InstanceID = i;
		InstancesDescArray[i].InstanceContributionToHitGroupIndex = 0;
		InstancesDescArray[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		InstancesDescArray[i].AccelerationStructure = CurMesh->BLAS.GetGPUAddress();
		InstancesDescArray[i].InstanceMask = 0xFF;

		XMFLOAT4X4 Mtx;
		CurMesh->GetWorldMatrix(&Mtx);
		memcpy(InstancesDescArray[i].Transform, &Mtx, sizeof(InstancesDescArray[i].Transform));
	}
	TLAS_Instances.SetData(InstancesDescArray.data());

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC AsDesc = {};
	AsDesc.Inputs = Inputs;
	AsDesc.Inputs.InstanceDescs = TLAS_Instances.GetGPUAddress();
	AsDesc.DestAccelerationStructureData = TLAS.GetGPUAddress();
	AsDesc.ScratchAccelerationStructureData = TLAS_Scratch.GetGPUAddress();

	InCommandList->BuildRaytracingAccelerationStructure(&AsDesc, 0, nullptr);

	CD3DX12_RESOURCE_BARRIER UavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(TLAS.GetResource());
	InCommandList->ResourceBarrier(1, &UavBarrier);
}