#include <iostream>
#include <filesystem>
#include "Material.h"
#include "Renderer.h"

void CTexture2D::CreateShaderResourceView()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Format = Texture->GetDesc().Format;
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MostDetailedMip = 0;
    SrvDesc.Texture2D.MipLevels = Texture->GetDesc().MipLevels;
    SrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    CD3DX12_CPU_DESCRIPTOR_HANDLE Descriptor = CRenderer::GetInstance().AllocSrvDescriptor(SrvDescriptorIndex);
    CRenderer::GetInstance().D3dDevice->CreateShaderResourceView(Texture.Get(), &SrvDesc, Descriptor);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CTexture2D::GetSrvGPUDescriptor()
{
    if (SrvDescriptorIndex < 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE DefaultHandle = {};
        return DefaultHandle;
    }

    return CRenderer::GetInstance().GetSrvGPUDescriptor(SrvDescriptorIndex);
}

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

    std::filesystem::path ExeDirectory = CRenderer::GetExeDirectory();

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

int CMaterial::FindSrvRootParameterIndex(UINT InRegister)
{
    int FoundRootParamIdx = -1;

    for (int i = 0; i < RootSignatureDesc.NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER& Param = RootSignatureDesc.pParameters[i];
        if (Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            if (Param.DescriptorTable.NumDescriptorRanges > 0
                && Param.DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_ROOT_PARAMETER_TYPE_SRV
                && Param.DescriptorTable.pDescriptorRanges[0].BaseShaderRegister == InRegister
                && Param.DescriptorTable.pDescriptorRanges[0].RegisterSpace == 0)
            {
                FoundRootParamIdx = i;
                break;
            }
        }
    }

    return FoundRootParamIdx;
}

void CMaterial::SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture2D* InTex)
{
    if (InTex == nullptr || InTex->HasValidSrv())
    {
        return;
    }

    int FoundRootParamIdx = FindSrvRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    InCommandList->SetGraphicsRootDescriptorTable(FoundRootParamIdx, InTex->GetSrvGPUDescriptor());
}

void CMaterial::SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CUniformBuffer* InBuffer)
{
    if (InBuffer == nullptr || InBuffer->HasValidSrv())
    {
        return;
    }

    int FoundRootParamIdx = FindSrvRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    InCommandList->SetGraphicsRootDescriptorTable(FoundRootParamIdx, InBuffer->GetSrvGPUDescriptor());
}