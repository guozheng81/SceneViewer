#include <iostream>
#include <filesystem>
#include "Material.h"
#include "Renderer.h"

CMaterial::CMaterial()
{
    PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    PSODesc.DepthStencilState.DepthEnable = FALSE;
    PSODesc.DepthStencilState.StencilEnable = FALSE;
    PSODesc.SampleMask = UINT_MAX;
    PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PSODesc.NumRenderTargets = 1;
    PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    PSODesc.SampleDesc.Count = 1;
}

void CMaterial::Build(LPCWSTR InVSFileName, LPCWSTR InPSFileName)
{
    ComPtr<ID3DBlob> SignBlob;
    ComPtr<ID3DBlob> ErrorBlob;
    D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &SignBlob, &ErrorBlob);
    CRenderer::GetInstance().D3dDevice->CreateRootSignature(0, SignBlob->GetBufferPointer(), SignBlob->GetBufferSize(), IID_PPV_ARGS(&RootSign));

	std::vector<D3D12_INPUT_ELEMENT_DESC> InputDescArray =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    ComPtr<ID3DBlob> VSBlob;
    ComPtr<ID3DBlob> PSBlob;

    WCHAR PathCharArray[MAX_PATH];
    GetModuleFileNameW(NULL, PathCharArray, MAX_PATH);
    std::filesystem::path ExePath(PathCharArray);
    std::filesystem::path ExeDirectory = ExePath.parent_path();

    D3DReadFileToBlob((ExeDirectory/InVSFileName).c_str(), &VSBlob);
    D3DReadFileToBlob((ExeDirectory/InPSFileName).c_str(), &PSBlob);

    PSODesc.InputLayout = { InputDescArray.data(), (UINT)(InputDescArray.size())};
    PSODesc.pRootSignature = RootSign.Get();
    PSODesc.VS = CD3DX12_SHADER_BYTECODE(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize());
    PSODesc.PS = CD3DX12_SHADER_BYTECODE(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize());
    CRenderer::GetInstance().D3dDevice->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&PSO));
}

void CMaterial::OnRender(ID3D12GraphicsCommandList* InCommandList)
{
    InCommandList->SetPipelineState(PSO.Get());
    InCommandList->SetGraphicsRootSignature(RootSign.Get());
}