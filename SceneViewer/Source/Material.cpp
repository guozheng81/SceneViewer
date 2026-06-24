#include <iostream>
#include <filesystem>
#include "Material.h"
#include "Renderer.h"

void CTexture2D::CreateShaderResourceView()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Format = Texture->GetDesc().Format;
    if (SrvDesc.Format == DXGI_FORMAT_R32_TYPELESS)
    {
        SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    }
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
    PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    PSODesc.DepthStencilState.StencilEnable = FALSE;
    PSODesc.SampleMask = UINT_MAX;
    PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PSODesc.NumRenderTargets = 1;
    PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    PSODesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    PSODesc.SampleDesc.Count = 1;
}

void CMaterial::IntRootParameters(UINT InCbvCount, UINT InSrvCount, UINT InRtvCount)
{
    RootParams.resize(InCbvCount + InSrvCount + InRtvCount);
    int RootIdx = 0;
    for (UINT CbvIdx = 0; CbvIdx < InCbvCount; ++CbvIdx, ++RootIdx)
    {
        RootParams[RootIdx].InitAsConstantBufferView(CbvIdx);
    }

    for (UINT RtvIdx = 0; RtvIdx < InRtvCount; ++RtvIdx, ++RootIdx)
    {
        RootParams[RootIdx].InitAsUnorderedAccessView(RtvIdx);
    }

    static CD3DX12_DESCRIPTOR_RANGE SrvRange;
    for (UINT SrvIdx = 0; SrvIdx < InSrvCount; ++SrvIdx, ++RootIdx)
    {
        SrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, SrvIdx, 0);

        RootParams[RootIdx].InitAsDescriptorTable(1, &SrvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    }
}

void CMaterial::Build(LPCWSTR InVSFileName, LPCWSTR InPSFileName)
{
    auto& Samplers = CRenderer::GetInstance().TextureSamplers;
    RootSignatureDesc.Init((UINT)(RootParams.size()), RootParams.data(), (UINT)(Samplers.size()), Samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> SignBlob;
    ComPtr<ID3DBlob> ErrorBlob;
    D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &SignBlob, &ErrorBlob);
    CRenderer::GetInstance().D3dDevice->CreateRootSignature(0, SignBlob->GetBufferPointer(), SignBlob->GetBufferSize(), IID_PPV_ARGS(&RootSign));

	std::vector<D3D12_INPUT_ELEMENT_DESC> InputDescArray =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
                && Param.DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV
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
    if (InTex == nullptr || !InTex->HasValidSrv())
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
    if (InBuffer == nullptr || !InBuffer->HasValidSrv())
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

void CMaterial::SetConstantBuffer(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CUniformBuffer* InBuffer)
{
    if (InBuffer == nullptr)
    {
        return;
    }

    int FoundRootParamIdx = -1;
    for (int i = 0; i < RootSignatureDesc.NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER& Param = RootSignatureDesc.pParameters[i];
        if (Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
        {
            if (Param.Descriptor.ShaderRegister == InRegister
                && Param.Descriptor.RegisterSpace == 0)
            {
                FoundRootParamIdx = i;
                break;
            }
        }
    }

    if (FoundRootParamIdx == -1)
    {
        return;
    }

    InCommandList->SetGraphicsRootConstantBufferView(FoundRootParamIdx, InBuffer->GetGPUAddress());
}