#include "Material.h"
#include "Renderer.h"

CMaterial::CMaterial(LPCWSTR InVSFileName, LPCWSTR InPSFileName, const D3D12_ROOT_SIGNATURE_DESC* InRootSignatureDesc)
{
    ComPtr<ID3DBlob> SignBlob;
    ComPtr<ID3DBlob> ErrorBlob;
    D3D12SerializeRootSignature(InRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &SignBlob, &ErrorBlob);
    CRenderer::GetInstance().D3dDevice->CreateRootSignature(0, SignBlob->GetBufferPointer(), SignBlob->GetBufferSize(), IID_PPV_ARGS(&RootSign));

	std::vector<D3D12_INPUT_ELEMENT_DESC> InputDescArray =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

    ComPtr<ID3DBlob> VSBlob;
    ComPtr<ID3DBlob> PSBlob;
    D3DReadFileToBlob(InVSFileName, &VSBlob);
    D3DReadFileToBlob(InPSFileName, &PSBlob);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = {};
    PsoDesc.InputLayout = { InputDescArray.data(), 3};
    PsoDesc.pRootSignature = RootSign.Get();
    PsoDesc.VS = CD3DX12_SHADER_BYTECODE(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize());
    PsoDesc.PS = CD3DX12_SHADER_BYTECODE(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize());
    PsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    PsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    PsoDesc.DepthStencilState.DepthEnable = FALSE;
    PsoDesc.DepthStencilState.StencilEnable = FALSE;
    PsoDesc.SampleMask = UINT_MAX;
    PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PsoDesc.NumRenderTargets = 1;
    PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    PsoDesc.SampleDesc.Count = 1;
    CRenderer::GetInstance().D3dDevice->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&PSO));
}