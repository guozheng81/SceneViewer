#include "Scene.h"

CScene::CScene()
{
	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc;
	RootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Material = std::make_shared<CMaterial>(L"Scene_VSMain.cso", L"Scene_PSMain.cso", &RootSignatureDesc);

	std::shared_ptr<CMesh> CurMesh = std::make_shared<CMesh>();
	AllMeshes.push_back(CurMesh);
}

CScene::~CScene()
{

}

void CScene::OnRender()
{
	Material->OnRender();

	for (auto CurMesh : AllMeshes)
	{
		CurMesh->OnRender();
	}
}