#include <iostream>
#include <filesystem>
#include "Material.h"
#include "Renderer.h"

DXGI_FORMAT ConvertUnormToSrgb(DXGI_FORMAT format)
{
    switch (format)
    {
        // Standard formats
        case DXGI_FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8X8_UNORM: return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

        // Block Compressed (BC) formats
        case DXGI_FORMAT_BC1_UNORM:      return DXGI_FORMAT_BC1_UNORM_SRGB;
        case DXGI_FORMAT_BC2_UNORM:      return DXGI_FORMAT_BC2_UNORM_SRGB;
        case DXGI_FORMAT_BC3_UNORM:      return DXGI_FORMAT_BC3_UNORM_SRGB;
        case DXGI_FORMAT_BC7_UNORM:      return DXGI_FORMAT_BC7_UNORM_SRGB;

        // Already SRGB or does not support hardware gamma mapping
        default:                         return format;
    }
}

CTexture2D::CTexture2D(bool InNeedRtv, bool InNeedUav, bool InIsDepth, bool InIsDiffuse)
    :bNeedRtv(InNeedRtv),
    bNeedUav(InNeedUav),
    bIsDepth(InIsDepth),
    bIsDiffuse(InIsDiffuse)
{

}

void CTexture2D::ResetUploadResource()
{
    if (UploadTexture)
    {
        UploadTexture.Reset();
        DDSData.reset();
    }
}

void CTexture2D::CreateShaderResourceView(bool bIsResizing)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Format = Texture->GetDesc().Format;
    if (bIsDepth)
    {
        SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    } else if (bIsDiffuse)
    {
        SrvDesc.Format = ConvertUnormToSrgb(SrvDesc.Format);
    }
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MostDetailedMip = 0;
    SrvDesc.Texture2D.MipLevels = Texture->GetDesc().MipLevels;
    SrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    if (!bIsResizing)
    {
        int		SrvDescriptorIndex = -1;
        SrvCPUDescriptor = CRenderer::GetInstance().AllocSrvDescriptor(SrvDescriptorIndex);
        SrvGPUDescriptor = CRenderer::GetInstance().GetSrvGPUDescriptor(SrvDescriptorIndex);
    }
        
    CRenderer::GetInstance().D3dDevice->CreateShaderResourceView(Texture.Get(), &SrvDesc, SrvCPUDescriptor);
}

void CTexture2D::CreateRenderTargetView(bool bIsResizing)
{
    if (!bIsResizing)
    {
        int RtvDescriptorIndex = -1;
        RtvCPUDescriptor = CRenderer::GetInstance().AllocRtvDescriptor(RtvDescriptorIndex);
    }
    CRenderer::GetInstance().D3dDevice->CreateRenderTargetView(Texture.Get(), nullptr, RtvCPUDescriptor);
}

void CTexture2D::CreateUnorderedAccessView(bool bIsResizing)
{
    if (!bIsResizing)
    {
        int		DescriptorIndex = -1;
        UavCPUDescriptor = CRenderer::GetInstance().AllocSrvDescriptor(DescriptorIndex);
        UavGPUDescriptor = CRenderer::GetInstance().GetSrvGPUDescriptor(DescriptorIndex);
    }

    CRenderer::GetInstance().D3dDevice->CreateUnorderedAccessView(Texture.Get(), nullptr, nullptr, UavCPUDescriptor);
}

void CTexture2D::OnResize(UINT InW, UINT InH)
{
    if (bIsDepth)
    {
        Texture.Reset();
        Width = InW;
        Height = InH;

        CreateDepthTextureResource();

        D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
        DsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // Cast from R32_TYPELESS
        DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        DsvDesc.Texture2D.MipSlice = 0;
        CRenderer::GetInstance().D3dDevice->CreateDepthStencilView(GetResource(), &DsvDesc, DsvCPUDescriptor);

        CreateShaderResourceView(true);
    }
    else if (bNeedRtv || bNeedUav)
    {
        DXGI_FORMAT RTFormat = Texture->GetDesc().Format;

        Texture.Reset();
        Width = InW;
        Height = InH;

        CreateRenderTargetResource(RTFormat, RTClearColor);
        if (bNeedRtv)
        {
            CreateRenderTargetView(true);
        }
        if (bNeedUav)
        {
            CreateUnorderedAccessView(true);
        }
        CreateShaderResourceView(true);
    }
}

void CTexture2D::CreateDepthTextureResource()
{
    D3D12_RESOURCE_DESC TextureDesc = {};
    TextureDesc.MipLevels = 1;
    TextureDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Use typeless so it can be cast to DSV and SRV
    TextureDesc.Width = Width;
    TextureDesc.Height = Height;
    TextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    TextureDesc.DepthOrArraySize = 1;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    ClearValue.DepthStencil.Depth = 1.0f;
    ClearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES HeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CRenderer::GetInstance().D3dDevice->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &ClearValue, IID_PPV_ARGS(Texture.GetAddressOf()));
}

void CTexture2D::CreateRenderTargetResource(DXGI_FORMAT InFormat, XMFLOAT4 InColor)
{
    D3D12_RESOURCE_DESC TextureDesc = {};

    TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    TextureDesc.Alignment = 0;
    TextureDesc.Width = Width;
    TextureDesc.Height = Height;
    TextureDesc.DepthOrArraySize = 1;
    TextureDesc.MipLevels = 1;
    TextureDesc.Format = InFormat;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;
    if (bNeedRtv)
    {
        Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (bNeedUav)
    {
        Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    TextureDesc.Flags = Flags;

    D3D12_HEAP_PROPERTIES HeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    RTClearColor = InColor;
    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = TextureDesc.Format;
    ClearValue.Color[0] = InColor.x;
    ClearValue.Color[1] = InColor.y;
    ClearValue.Color[2] = InColor.z;
    ClearValue.Color[3] = InColor.w;

    CRenderer::GetInstance().D3dDevice->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_COMMON, (bNeedRtv? &ClearValue : nullptr), IID_PPV_ARGS(Texture.GetAddressOf()));

}

///////////////////////////////////////////

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

void CMaterial::IntRootParameters(UINT InCbvCount, UINT InSrvCount, UINT InUavCount, std::vector<CD3DX12_ROOT_PARAMETER>& RootParams, std::vector<CD3DX12_DESCRIPTOR_RANGE>& Ranges)
{
    RootParams.resize(InCbvCount + InSrvCount + InUavCount);
    int RootIdx = 0;
    for (UINT CbvIdx = 0; CbvIdx < InCbvCount; ++CbvIdx, ++RootIdx)
    {
        RootParams[RootIdx].InitAsConstantBufferView(CbvIdx);
    }

    int RangeNum = (InSrvCount + InUavCount);
    if (RangeNum > 0)
    {
        Ranges.resize(RangeNum);
    }

    for (UINT SrvIdx = 0; SrvIdx < InSrvCount; ++SrvIdx, ++RootIdx)
    {
        Ranges[SrvIdx].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, SrvIdx, 0);

        RootParams[RootIdx].InitAsDescriptorTable(1, &(Ranges[SrvIdx]), D3D12_SHADER_VISIBILITY_ALL);
    }

    for (UINT UavIdx = 0; UavIdx < InUavCount; ++UavIdx, ++RootIdx)
    {
        int Idx = InSrvCount + UavIdx;
        Ranges[Idx].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, UavIdx, 0);

        RootParams[RootIdx].InitAsDescriptorTable(1, &(Ranges[Idx]), D3D12_SHADER_VISIBILITY_ALL);
    }
}

void CMaterial::BuildRootSignature(std::vector<CD3DX12_ROOT_PARAMETER>& InRootParams, bool bInForRaytracing)
{
    bUsedForRaytracing = bInForRaytracing;

    auto& Samplers = CRenderer::GetInstance().TextureSamplers;
    CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
    RootSignatureDesc.Init((UINT)(InRootParams.size()), InRootParams.data(), (UINT)(Samplers.size()), Samplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);

    if (bInForRaytracing)
    {
        RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
    }

    ComPtr<ID3DBlob> SignBlob;
    ComPtr<ID3DBlob> ErrorBlob;
    D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &SignBlob, &ErrorBlob);
    CRenderer::GetInstance().D3dDevice->CreateRootSignature(0, SignBlob->GetBufferPointer(), SignBlob->GetBufferSize(), IID_PPV_ARGS(&RootSign));

    for (int i = 0; i < InRootParams.size(); ++i)
    {
        const D3D12_ROOT_PARAMETER& Param = InRootParams[i];
        if (Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
        {
            UINT Space = Param.Descriptor.RegisterSpace;
            if (ConstantRegisterMap.size() >= Space)
            {
                ConstantRegisterMap.resize(Space + 1);
            }

            ConstantRegisterMap[Space][Param.Descriptor.ShaderRegister] = i;
        }
        else if (Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
        {
            UINT Space = Param.Constants.RegisterSpace;
            if (ConstantRegisterMap.size() >= Space)
            {
                ConstantRegisterMap.resize(Space + 1);
            }

            ConstantRegisterMap[Space][Param.Constants.ShaderRegister] = i;
        }
        else if (Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            if (Param.DescriptorTable.NumDescriptorRanges > 0)
            {
                if (Param.DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                {
                    UINT Space = Param.DescriptorTable.pDescriptorRanges[0].RegisterSpace;
                    if (SrvRegisterMap.size() >= Space)
                    {
                        SrvRegisterMap.resize(Space + 1);
                    }

                    SrvRegisterMap[Space][Param.DescriptorTable.pDescriptorRanges[0].BaseShaderRegister] = i;
                }
                else if (Param.DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                {
                    UINT Space = Param.DescriptorTable.pDescriptorRanges[0].RegisterSpace;
                    if (UavRegisterMap.size() >= Space)
                    {
                        UavRegisterMap.resize(Space + 1);
                    }

                    UavRegisterMap[Space][Param.DescriptorTable.pDescriptorRanges[0].BaseShaderRegister] = i;
                }
            }
        }
    }
}

void CMaterial::BuildPSO(LPCWSTR InVSFileName, LPCWSTR InPSFileName)
{
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

void CMaterial::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
    if (bUsedForRaytracing)
    {
        InCommandList->SetPipelineState1(RaytracingPSO.Get());
        InCommandList->SetComputeRootSignature(RootSign.Get());
    }
    else
    {
        InCommandList->SetPipelineState(PSO.Get());
        InCommandList->SetGraphicsRootSignature(RootSign.Get());
    }
}

int CMaterial::FindSrvRootParameterIndex(UINT InRegister, UINT InSpace)
{
    if (InSpace >= SrvRegisterMap.size())
    {
        return -1;
    }

    auto Iter = SrvRegisterMap[InSpace].find(InRegister);
    if (Iter != SrvRegisterMap[InSpace].end())
    {
        return Iter->second;
    }
    return -1;
}

int CMaterial::FindConstantRootParameterIndex(UINT InRegister, UINT InSpace)
{
    if (InSpace >= ConstantRegisterMap.size())
    {
        return -1;
    }

    auto Iter = ConstantRegisterMap[InSpace].find(InRegister);
    if (Iter != ConstantRegisterMap[InSpace].end())
    {
        return Iter->second;
    }
    return -1;
}

int CMaterial::FindUavRootParameterIndex(UINT InRegister, UINT InSpace)
{
    if (InSpace >= UavRegisterMap.size())
    {
        return -1;
    }

    auto Iter = UavRegisterMap[InSpace].find(InRegister);
    if (Iter != UavRegisterMap[InSpace].end())
    {
        return Iter->second;
    }
    return -1;
}

void CMaterial::SetUav(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture2D* InTex)
{
    if (InTex == nullptr || InTex->UavGPUDescriptor.ptr == 0)
    {
        return;
    }

    int FoundRootParamIdx = FindUavRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    if (bUsedForRaytracing)
    {
        InCommandList->SetComputeRootDescriptorTable(FoundRootParamIdx, InTex->UavGPUDescriptor);
    }
    else
    {
        InCommandList->SetGraphicsRootDescriptorTable(FoundRootParamIdx, InTex->UavGPUDescriptor);
    }
}

void CMaterial::SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture2D* InTex)
{
    if (InTex == nullptr || InTex->SrvGPUDescriptor.ptr == 0)
    {
        return;
    }

    int FoundRootParamIdx = FindSrvRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    if (bUsedForRaytracing)
    {
        InCommandList->SetComputeRootDescriptorTable(FoundRootParamIdx, InTex->SrvGPUDescriptor);
    }
    else
    {
        InCommandList->SetGraphicsRootDescriptorTable(FoundRootParamIdx, InTex->SrvGPUDescriptor);
    }
}

void CMaterial::SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CBuffer* InBuffer)
{
    if (InBuffer == nullptr || InBuffer->SrvGPUDescriptor.ptr == 0)
    {
        return;
    }

    int FoundRootParamIdx = FindSrvRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    if (bUsedForRaytracing)
    {
        InCommandList->SetComputeRootDescriptorTable(FoundRootParamIdx, InBuffer->SrvGPUDescriptor);
    }
    else
    {
        InCommandList->SetGraphicsRootDescriptorTable(FoundRootParamIdx, InBuffer->SrvGPUDescriptor);
    }
}

void CMaterial::SetConstantBuffer(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CBuffer* InBuffer)
{
    if (InBuffer == nullptr)
    {
        return;
    }

    int FoundRootParamIdx = FindConstantRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    if (bUsedForRaytracing)
    {
        InCommandList->SetComputeRootConstantBufferView(FoundRootParamIdx, InBuffer->GetGPUAddress());
    }
    else
    {
        InCommandList->SetGraphicsRootConstantBufferView(FoundRootParamIdx, InBuffer->GetGPUAddress());
    }
}

void CMaterial::BuildRaytracingPSO(LPCWSTR InFileName, LPCWSTR InRayGenName)
{
    CD3DX12_STATE_OBJECT_DESC RtPSODesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    auto DxilLib = RtPSODesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

    ComPtr<ID3DBlob> ShaderBlob;
    std::filesystem::path ExeDirectory = CRenderer::GetExeDirectory();

    D3DReadFileToBlob((ExeDirectory / InFileName).c_str(), &ShaderBlob);
    D3D12_SHADER_BYTECODE dxilBytecode = { ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize()};
    DxilLib->SetDXILLibrary(&dxilBytecode);

    DxilLib->DefineExport(InRayGenName);
    DxilLib->DefineExport(L"PrimaryMiss");
    DxilLib->DefineExport(L"PrimaryClosestHit");

    auto HitGroup = RtPSODesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    HitGroup->SetHitGroupExport(L"PrimaryHitGroup");
    HitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    HitGroup->SetClosestHitShaderImport(L"PrimaryClosestHit");
    // HitGroup->SetAnyHitShaderImport(L"PrimaryAnyHit"); // Optional

    auto ShaderConfig = RtPSODesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    ShaderConfig->Config(sizeof(float) * 4, sizeof(float) * 2);

    auto GlobalRootSigSubobject = RtPSODesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    GlobalRootSigSubobject->SetRootSignature(RootSign.Get());

    auto PipelineConfig = RtPSODesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();

    UINT MaxRecursionDepth = 1;
    PipelineConfig->Config(MaxRecursionDepth);

    CRenderer::GetInstance().D3dDevice->CreateStateObject(RtPSODesc, IID_PPV_ARGS(&RaytracingPSO));

    RaytracingPSO->QueryInterface(IID_PPV_ARGS(&RtPSOProperties));
}

void* CMaterial::GetRaytracingShaderIdentifier(LPCWSTR InName)
{
    return RtPSOProperties->GetShaderIdentifier(InName);
}