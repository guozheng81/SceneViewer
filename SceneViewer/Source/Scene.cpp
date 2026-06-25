#include "Scene.h"
#include "Renderer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

void CMesh::Init(std::vector<SSceneVertex>& Verts, std::vector<UINT32>& Indices, const std::string InDiffTexName)
{
	VertexCount = Verts.size();
	UINT TotalSize = sizeof(SSceneVertex)* VertexCount;
	VertexBuffer = CRenderer::GetInstance().CreateDefaultBuffer(Verts.data(), TotalSize, VertexUploadBuffer);

	VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
	VertexBufferView.SizeInBytes = TotalSize;
	VertexBufferView.StrideInBytes = sizeof(SSceneVertex);

	IndicesCount = Indices.size();
	if (IndicesCount > 0)
	{
		TotalSize = sizeof(UINT32) * IndicesCount;
		IndexBuffer = CRenderer::GetInstance().CreateDefaultBuffer(Indices.data(), TotalSize, IndexUploadBuffer);
		IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
		IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		IndexBufferView.SizeInBytes = TotalSize;
	}

	DiffuseTextureName = InDiffTexName;
	CRenderer::GetInstance().LoadTexture(DiffuseTextureName);
}

void CMesh::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	InCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	InCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	if (IndicesCount > 0)
	{
		InCommandList->IASetIndexBuffer(&IndexBufferView);
		InCommandList->DrawIndexedInstanced(IndicesCount, 1, 0, 0, 0);
	}
	else
	{
		InCommandList->DrawInstanced(VertexCount, 1, 0, 0);
	}
}

CScene::CScene()
{
	MainCamera.SetAspectRatio(CRenderer::GetInstance().ViewportWidth, CRenderer::GetInstance().ViewportHeight);
	MainCamera.SetFOV(55.0f);
	MainCamera.SetPositionAndRotation(XMFLOAT3(0.0f, 200.0f, 0.0f), XMConvertToRadians(-90.0f), 0.0f);

	Material = std::make_unique<CMaterial>();
	//Material->PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
}

void CScene::Load()
{
	Material->IntRootParameters(1, 1, 0);
	Material->Build(L"Scene_VSMain.cso", L"Scene_PSMain.cso");

	/*
	std::vector<SSceneVertex> Verts = {
			{ { 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
			{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} }
	};

	std::vector<UINT32>	Indices = { 0, 1, 2 };
	*/

	std::filesystem::path AssetPath = CRenderer::GetAssetDirectory();
	AssetPath /= "sponza.obj";

	tinyobj::ObjReaderConfig ReaderConfig;
	ReaderConfig.mtl_search_path = "";

	tinyobj::ObjReader TinyObjReader;

	std::vector<SSceneVertex> Verts;
	std::vector<UINT32>	Indices;

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
					std::unique_ptr<CMesh> CurMesh = std::make_unique<CMesh>();
					CurMesh->Init(Verts, Indices, TinyObjMat.diffuse_texname);
					AllMeshes.push_back(std::move(CurMesh));

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
					tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

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
			std::unique_ptr<CMesh> CurMesh = std::make_unique<CMesh>();
			CurMesh->Init(Verts, Indices, TinyObjMat.diffuse_texname);
			AllMeshes.push_back(std::move(CurMesh));
		}
	}
}

CMaterial* CScene::GetSceneMaterial()
{
	return Material.get();
}

CScene::~CScene()
{

}

void CScene::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
	for (auto& CurMesh : AllMeshes)
	{
		Material->SetShaderResource(InCommandList, 0, CRenderer::GetInstance().GetTexture(CurMesh->DiffuseTextureName));
		CurMesh->OnRender(InCommandList);
	}
}