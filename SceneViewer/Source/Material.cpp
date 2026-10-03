#include <iostream>
#include <filesystem>
#include <set>
#include <map>
#include <algorithm>

#include "Material.h"
#include "Renderer.h"
#include "Scene.h"
#include "Logger.h"

bool IsSrvInputType(D3D_SHADER_INPUT_TYPE InType)
{
    switch (InType)
    {
    case D3D_SIT_TBUFFER:
    case D3D_SIT_TEXTURE:
    case D3D_SIT_STRUCTURED:
    case D3D_SIT_BYTEADDRESS:
    case D3D_SIT_RTACCELERATIONSTRUCTURE:
        return true;
    default:
        return false;
    }
}

bool IsUavInputType(D3D_SHADER_INPUT_TYPE InType)
{
    switch (InType)
    {
    case D3D_SIT_UAV_RWTYPED:
    case D3D_SIT_UAV_RWSTRUCTURED:
    case D3D_SIT_UAV_RWBYTEADDRESS:
    case D3D_SIT_UAV_APPEND_STRUCTURED:
    case D3D_SIT_UAV_CONSUME_STRUCTURED:
    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
    case D3D_SIT_UAV_FEEDBACKTEXTURE:
        return true;
    default:
        return false;
    }
}

bool AccumulateNonCbvBinding(
    const D3D12_SHADER_INPUT_BIND_DESC& InBindingDesc,
    UINT& OutSrvCount,
    UINT& OutUavCount,
    std::set<UINT>& OutUnboundSrvSpaces)
{
    if (IsSrvInputType(InBindingDesc.Type))
    {
        const bool bIsUnbound = (InBindingDesc.BindCount == 0 || InBindingDesc.BindCount == UINT_MAX);
        if (bIsUnbound)
        {
            if (InBindingDesc.BindPoint != 0 || InBindingDesc.Space == 0)
            {
                LOG_ERROR("Unbound SRV '%s' uses t%u space%u. Expected t0 in space 1+.",
                    InBindingDesc.Name, InBindingDesc.BindPoint, InBindingDesc.Space);
                return false;
            }

            OutUnboundSrvSpaces.insert(InBindingDesc.Space);
        }
        else
        {
            if (InBindingDesc.Space != 0)
            {
                LOG_ERROR("Bound SRV '%s' uses space %u. Expected SRV space 0.",
                    InBindingDesc.Name, InBindingDesc.Space);
                return false;
            }

            UINT RequiredCount = InBindingDesc.BindPoint + InBindingDesc.BindCount;
            if (RequiredCount > OutSrvCount)
            {
                OutSrvCount = RequiredCount;
            }
        }

        return true;
    }

    if (IsUavInputType(InBindingDesc.Type))
    {
        if (InBindingDesc.Space != 0)
        {
            LOG_ERROR("UAV '%s' uses space %u. Expected UAV space 0.",
                InBindingDesc.Name, InBindingDesc.Space);
            return false;
        }

        UINT RegisterSpan = (InBindingDesc.BindCount == 0 || InBindingDesc.BindCount == UINT_MAX)
            ? 1
            : InBindingDesc.BindCount;
        UINT RequiredCount = InBindingDesc.BindPoint + RegisterSpan;
        if (RequiredCount > OutUavCount)
        {
            OutUavCount = RequiredCount;
        }

        return true;
    }

    return true;
}

UINT GetRootConstantsCount(const std::string& InName, ID3D12ShaderReflectionConstantBuffer* ConstBuffer)
{
    if(InName.find("Constants_") != 0)
    {
        //LOG_ERROR("GetRootConstantsCount: Constant buffer name '%s' does not start with 'Constants_'.", InName.c_str());
        return 0;
	}

    D3D12_SHADER_BUFFER_DESC BufferDesc = {};
    HRESULT HrBufferDesc = ConstBuffer->GetDesc(&BufferDesc);
    if (FAILED(HrBufferDesc))
    {
		LOG_ERROR("GetRootConstantsCount: Failed to get constant buffer description (0x%08X).", HrBufferDesc);
        return 0;
    }

    UINT TotalRequiredBytes = 0;

    // Loop through every variable to find the absolute furthest byte used
    for (UINT i = 0; i < BufferDesc.Variables; ++i) {
        ID3D12ShaderReflectionVariable* pVar = ConstBuffer->GetVariableByIndex(i);
        D3D12_SHADER_VARIABLE_DESC VarDesc;
        pVar->GetDesc(&VarDesc);

        UINT variableEndByte = VarDesc.StartOffset + VarDesc.Size;
        if (variableEndByte > TotalRequiredBytes) {
            TotalRequiredBytes = variableEndByte;
        }
    }

    return (TotalRequiredBytes + 3) / 4;
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

/// <summary>
/// Initializes root parameters and descriptor ranges for the root signature.
/// 
/// This method constructs the root signature parameter layout by organizing constant buffer views (CBVs),
/// shader resource views (SRVs), unordered access views (UAVs), and unbound SRVs into descriptor tables
/// and root parameters. The resulting structures are used when building the root signature for graphics
/// or compute pipelines.
/// </summary>
/// <param name="InCbvCount">Number of constant buffer view parameters to create. Each CBV is added as
/// a direct root parameter (not in a descriptor table).</param>
/// <param name="InSrvCount">Number of bound shader resource view parameters to create. Each SRV is
/// placed in its own descriptor table with a single descriptor range.</param>
/// <param name="InUavCount">Number of unordered access view parameters to create. Each UAV is placed
/// in its own descriptor table with a single descriptor range.</param>
/// <param name="InUnboundSrvCount">Number of unbound shader resource view parameters to create. Unbound
/// SRVs allow access to an unbounded range of resources (descriptor range with count -1).</param>
/// <param name="RootParams">Output vector that will be resized and populated with root parameters.
/// The order is: CBVs, then SRVs, then UAVs, then unbound SRVs.</param>
/// <param name="Ranges">Output vector that will be resized and populated with descriptor ranges for
/// all SRVs, UAVs, and unbound SRVs. CBVs do not use descriptor ranges.</param>
void CMaterial::InitRootParameters(UINT InCbvCount, UINT InSrvCount, UINT InUavCount, UINT InUnboundSrvCount, std::vector<CD3DX12_ROOT_PARAMETER>& RootParams, std::vector<CD3DX12_DESCRIPTOR_RANGE>& Ranges)
{
    const UINT MAX_ROOT_PARAMETERS = 64; // D3D12 limit is 64 DWORD slots; conservative estimate
    UINT TotalParams = InCbvCount + InSrvCount + InUavCount + InUnboundSrvCount;

    if (TotalParams > MAX_ROOT_PARAMETERS)
    {
		LOG_ERROR("Total root parameters (%u) exceed the maximum allowed (%u).", TotalParams, MAX_ROOT_PARAMETERS);
        return;
	}

    RootParams.resize(TotalParams);
    int RootIdx = 0;

    // Initialize CBVs as direct root parameters (most efficient)
    for (UINT CbvIdx = 0; CbvIdx < InCbvCount; ++CbvIdx, ++RootIdx)
    {
        RootParams[RootIdx].InitAsConstantBufferView(CbvIdx);
    }

    int RangeNum = (InSrvCount + InUavCount + InUnboundSrvCount);
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

    // Initialize unbound SRVs (unbounded arrays) in descriptor tables
    // These use register space 1+ to avoid conflicts with other bindings
    for (UINT UnboundIdx = 0; UnboundIdx < InUnboundSrvCount; ++UnboundIdx, ++RootIdx)
    {
        int Idx = InSrvCount + InUavCount + UnboundIdx;
        Ranges[Idx].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 0, UnboundIdx + 1);
        RootParams[RootIdx].InitAsDescriptorTable(1, &(Ranges[Idx]), D3D12_SHADER_VISIBILITY_ALL);
    }
}

void CMaterial::InitRootParametersFromShaders(const std::vector<ID3DBlob*>& InShaderBlobs, std::vector<CD3DX12_ROOT_PARAMETER>& RootParams, std::vector<CD3DX12_DESCRIPTOR_RANGE>& Ranges)
{
    UINT CbvCount = 0;
    UINT SrvCount = 0;
    UINT UavCount = 0;
    UINT UnboundSrvCount = 0;
    std::vector<UINT> RootConstantsCountPerRegister;
    if (!CMaterial::CollectRootParameterCountsFromShaders(
        InShaderBlobs,
        CbvCount,
        SrvCount,
        UavCount,
        UnboundSrvCount,
        RootConstantsCountPerRegister))
    {
        LOG_ERROR("CMaterial::InitRootParametersFromShaders: Failed to collect root parameter counts from shader blobs.");
        return;
    }

    CMaterial::InitRootParameters(CbvCount, SrvCount, UavCount, UnboundSrvCount, RootParams, Ranges);

    for (UINT Register = 0; Register < (UINT)RootConstantsCountPerRegister.size(); ++Register)
    {
        UINT ConstantsCount = RootConstantsCountPerRegister[Register];
        if (ConstantsCount == 0)
        {
            if(Register + 1 <(UINT)RootConstantsCountPerRegister.size() && RootConstantsCountPerRegister[Register+1] > 0 && CbvCount != Register + 1)
            {
                LOG_ERROR("Root constants found in register space %u, but CBV count is %u. Expected CBV registers before root constants register.", Register+1, CbvCount);
			}

            continue;
        }

        CD3DX12_ROOT_PARAMETER RootConstantParam;
        RootConstantParam.InitAsConstants(ConstantsCount, Register);
        RootParams.push_back(RootConstantParam);
    }
}

void CMaterial::BuildRootSignature(std::vector<CD3DX12_ROOT_PARAMETER>& InRootParams, bool bInForRaytracing)
{
    if (InRootParams.empty())
    {
        LOG_ERROR("BuildRootSignature called with empty root parameters.");
        return;
    }

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
    HRESULT HrSerialize = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &SignBlob, &ErrorBlob);
    if (FAILED(HrSerialize))
    {
        const char* ErrorMsg = ErrorBlob ? (const char*)ErrorBlob->GetBufferPointer() : "Unknown error";
        LOG_ERROR("D3D12SerializeRootSignature failed (0x%08X): %s", HrSerialize, ErrorMsg);
        return;
    }

    HRESULT HrCreateSig = CRenderer::GetInstance().D3dDevice->CreateRootSignature(0, SignBlob->GetBufferPointer(), SignBlob->GetBufferSize(), IID_PPV_ARGS(&RootSign));
    if (FAILED(HrCreateSig))
    {
        LOG_ERROR("CreateRootSignature failed (0x%08X).", HrCreateSig);
        return;
    }

    for (int i = 0; i < InRootParams.size(); ++i)
    {
        const D3D12_ROOT_PARAMETER& Param = InRootParams[i];
        if (Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
        {
            UINT Space = Param.Descriptor.RegisterSpace;
            if (ConstantRegisterMap.size() <= Space)
            {
                ConstantRegisterMap.resize(Space + 1);
            }

            ConstantRegisterMap[Space][Param.Descriptor.ShaderRegister] = i;
        }
        else if (Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
        {
            UINT Space = Param.Constants.RegisterSpace;
            if (ConstantRegisterMap.size() <= Space)
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
                    if (SrvRegisterMap.size() <= Space)
                    {
                        SrvRegisterMap.resize(Space + 1);
                    }

                    SrvRegisterMap[Space][Param.DescriptorTable.pDescriptorRanges[0].BaseShaderRegister] = i;
                }
                else if (Param.DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                {
                    UINT Space = Param.DescriptorTable.pDescriptorRanges[0].RegisterSpace;
                    if (UavRegisterMap.size() <= Space)
                    {
                        UavRegisterMap.resize(Space + 1);
                    }

                    UavRegisterMap[Space][Param.DescriptorTable.pDescriptorRanges[0].BaseShaderRegister] = i;
                }
            }
        }
    }
}

void CMaterial::BuildComputePSO(ComPtr<ID3DBlob> CSBlob)
{
    if (!RootSign.Get())
    {
        LOG_ERROR("BuildComputePSO: Root signature is not initialized. Call BuildRootSignature first.");
        return;
	}

    D3D12_COMPUTE_PIPELINE_STATE_DESC ComputePsoDesc = {};

    ComputePsoDesc.pRootSignature = RootSign.Get();
    ComputePsoDesc.CS = { CSBlob->GetBufferPointer(), CSBlob->GetBufferSize() };
    ComputePsoDesc.NodeMask = 0;
    ComputePsoDesc.CachedPSO = {};
    ComputePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    HRESULT HrState = CRenderer::GetInstance().D3dDevice->CreateComputePipelineState(&ComputePsoDesc, IID_PPV_ARGS(&PSO));
    if(FAILED(HrState))
    {
        LOG_ERROR("BuildComputePSO: CreateComputePipelineState failed (0x%08X).", HrState);
        PSO.Reset();
        return;
	}

    bUsedForCompute = true;
}

void CMaterial::BuildPSO(ComPtr<ID3DBlob> VSBlob, ComPtr<ID3DBlob> PSBlob)
{
    if (!RootSign.Get())
    {
        LOG_ERROR("BuildPSO: Root signature is not initialized. Call BuildRootSignature first.");
        return;
    }

	static const std::vector<D3D12_INPUT_ELEMENT_DESC> InputDescArray =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    if (!VSBlob || VSBlob->GetBufferSize() == 0)
    {
        LOG_ERROR("BuildPSO: Vertex shader blob is invalid or empty.");
        return;
    }

    if (!PSBlob || PSBlob->GetBufferSize() == 0)
    {
        LOG_ERROR("BuildPSO: Pixel shader blob is invalid or empty.");
        return;
    }

    PSODesc.InputLayout = { InputDescArray.data(), (UINT)(InputDescArray.size())};
    PSODesc.pRootSignature = RootSign.Get();
    PSODesc.VS = CD3DX12_SHADER_BYTECODE(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize());
    PSODesc.PS = CD3DX12_SHADER_BYTECODE(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize());
    HRESULT HrCreatePSO = CRenderer::GetInstance().D3dDevice->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&PSO));
    if (FAILED(HrCreatePSO))
    {
        LOG_ERROR("BuildPSO: CreateGraphicsPipelineState failed (0x%08X).", HrCreatePSO);
        PSO.Reset();
    }
}

void CMaterial::OnRender(ID3D12GraphicsCommandList4* InCommandList)
{
    if (bUsedForRaytracing)
    {
        InCommandList->SetPipelineState1(RaytracingPSO.Get());
        InCommandList->SetComputeRootSignature(RootSign.Get());

        RaytraceDesc.Width = CRenderer::GetInstance().ViewportWidth;
        RaytraceDesc.Height = CRenderer::GetInstance().ViewportHeight;
        RaytraceDesc.Depth = 1;
    }
    else if (bUsedForCompute)
    {
        InCommandList->SetPipelineState(PSO.Get());
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

	LOG_ERROR("FindSrvRootParameterIndex: No root parameter found for SRV register %u in space %u.", InRegister, InSpace);
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

	LOG_ERROR("FindConstantRootParameterIndex: No root parameter found for constant buffer register %u in space %u.", InRegister, InSpace);
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

	LOG_ERROR("FindUavRootParameterIndex: No root parameter found for UAV register %u in space %u.", InRegister, InSpace);
    return -1;
}

void CMaterial::SetUav(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture* InTex)
{
    if (InTex == nullptr)
    {
        LOG_ERROR("SetUav: Invalid texture for register %u.", InRegister);
		return;
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE UavHandle = InTex->GetUavGPUDescriptor();
    if (UavHandle.ptr == 0)
    {
		LOG_ERROR("SetUav: Invalid UAV descriptor for register %u.", InRegister);
        return;
    }

    int FoundRootParamIdx = FindUavRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    if (bUsedForRaytracing || bUsedForCompute)
    {
        InCommandList->SetComputeRootDescriptorTable(FoundRootParamIdx, UavHandle);
    }
    else
    {
        InCommandList->SetGraphicsRootDescriptorTable(FoundRootParamIdx, UavHandle);
    }
}

void CMaterial::SetShaderResource(ID3D12GraphicsCommandList* InCommandList, UINT InRegister, CTexture* InTex)
{
    if (InTex == nullptr || InTex->SrvGPUDescriptor.ptr == 0)
    {
		LOG_ERROR("SetShaderResource: Invalid texture or SRV descriptor for register %u.", InRegister);
        return;
    }

    int FoundRootParamIdx = FindSrvRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    if (bUsedForRaytracing || bUsedForCompute)
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
		LOG_ERROR("SetShaderResource: Invalid buffer or SRV descriptor for register %u.", InRegister);
        return;
    }

    if (InBuffer->IsConstantBuffer())
    {
        LOG_ERROR("SetShaderResource: The provided buffer is a constant buffer. Use SetConstantBuffer instead.");
		return;
    }

    int FoundRootParamIdx = FindSrvRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    if (bUsedForRaytracing || bUsedForCompute)
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
		LOG_ERROR("SetConstantBuffer: Invalid buffer for register %u.", InRegister);
        return;
    }

    if(!(InBuffer->IsConstantBuffer()))
    {
        LOG_ERROR("SetConstantBuffer: The provided buffer is not a constant buffer.");
        return;
	}

    int FoundRootParamIdx = FindConstantRootParameterIndex(InRegister);
    if (FoundRootParamIdx == -1)
    {
        return;
    }

    if (bUsedForRaytracing || bUsedForCompute)
    {
        InCommandList->SetComputeRootConstantBufferView(FoundRootParamIdx, InBuffer->GetGPUAddress());
    }
    else
    {
        InCommandList->SetGraphicsRootConstantBufferView(FoundRootParamIdx, InBuffer->GetGPUAddress());
    }
}

void CMaterial::BuildRaytracingPSO(ComPtr<ID3DBlob> ShaderBlob, LPCWSTR InRayGenName, const std::vector<SRaytracingShaderInfo>& InShaderInfoArray, UINT MaxRecursionDepth)
{
    if(ShaderBlob == nullptr || ShaderBlob->GetBufferSize() == 0)
    {
        LOG_ERROR("BuildRaytracingPSO: Invalid shader blob.");
        return;
	}

    CD3DX12_STATE_OBJECT_DESC RtPSODesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    auto DxilLib = RtPSODesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

    D3D12_SHADER_BYTECODE dxilBytecode = { ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize()};
    DxilLib->SetDXILLibrary(&dxilBytecode);

    UINT RayTypeCount = InShaderInfoArray.size();

    DxilLib->DefineExport(InRayGenName);

    for (UINT TypeIdx = 0; TypeIdx < RayTypeCount; ++TypeIdx)
    {
        DxilLib->DefineExport(InShaderInfoArray[TypeIdx].MissShader.c_str());
        DxilLib->DefineExport(InShaderInfoArray[TypeIdx].ClosestHitShader.c_str());
        bool bHasAnyHit = (!InShaderInfoArray[TypeIdx].AnyHitShader.empty());
        if (bHasAnyHit)
        {
            DxilLib->DefineExport(InShaderInfoArray[TypeIdx].AnyHitShader.c_str());
        }

        auto HitGroup = RtPSODesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        HitGroup->SetHitGroupExport(InShaderInfoArray[TypeIdx].HitGroup.c_str());
        HitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
        HitGroup->SetClosestHitShaderImport(InShaderInfoArray[TypeIdx].ClosestHitShader.c_str());
        if (bHasAnyHit)
        {
            HitGroup->SetAnyHitShaderImport(InShaderInfoArray[TypeIdx].AnyHitShader.c_str()); // Optional
        }
    }

    auto ShaderConfig = RtPSODesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    ShaderConfig->Config(sizeof(float) * 4, sizeof(float) * 2);

    auto GlobalRootSigSubobject = RtPSODesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    GlobalRootSigSubobject->SetRootSignature(RootSign.Get());

    auto PipelineConfig = RtPSODesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();

    PipelineConfig->Config(MaxRecursionDepth);

    HRESULT HrState = CRenderer::GetInstance().D3dDevice->CreateStateObject(RtPSODesc, IID_PPV_ARGS(&RaytracingPSO));
    if (FAILED(HrState))
    {
        LOG_ERROR("BuildRaytracingPSO: CreateStateObject failed (0x%08X).", HrState);
        RaytracingPSO.Reset();
        return;
    }

    RaytracingPSO->QueryInterface(IID_PPV_ARGS(&RtPSOProperties));

    ShaderBindingTable.Init(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, RayTypeCount*2 + 1, true);
    ShaderBindingTable.SetElementData(0, RtPSOProperties->GetShaderIdentifier(InRayGenName), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    for (UINT TypeIdx = 0; TypeIdx < RayTypeCount; ++TypeIdx)
    {
        ShaderBindingTable.SetElementData(TypeIdx + 1, RtPSOProperties->GetShaderIdentifier(InShaderInfoArray[TypeIdx].MissShader.c_str()), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        ShaderBindingTable.SetElementData(TypeIdx + RayTypeCount +1 , RtPSOProperties->GetShaderIdentifier(InShaderInfoArray[TypeIdx].HitGroup.c_str()), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }

    RaytraceDesc.RayGenerationShaderRecord.StartAddress = ShaderBindingTable.GetGPUAddress(0);
    RaytraceDesc.RayGenerationShaderRecord.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

    RaytraceDesc.MissShaderTable.StartAddress = ShaderBindingTable.GetGPUAddress(1);
    RaytraceDesc.MissShaderTable.StrideInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
    RaytraceDesc.MissShaderTable.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT*RayTypeCount;

    RaytraceDesc.HitGroupTable.StartAddress = ShaderBindingTable.GetGPUAddress(RayTypeCount+1);
    RaytraceDesc.HitGroupTable.StrideInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
    RaytraceDesc.HitGroupTable.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT*RayTypeCount;
}

void CMaterial::SetSceneForRaytracing(ID3D12GraphicsCommandList* InCommandList, CScene* InScene)
{
	CBuffer* ModelBuffer = InScene->GetModelBuffer();
    if (ModelBuffer)
    {
        SetShaderResource(InCommandList, 0, ModelBuffer);
    }
    int TLASParam = FindSrvRootParameterIndex(1);
    if (TLASParam >= 0)
    {
        InCommandList->SetComputeRootDescriptorTable(TLASParam, InScene->TLASGPUDescriptor);
    }

    int TexturesParam = FindSrvRootParameterIndex(0, 1);
    if (TexturesParam >= 0)
    {
        InCommandList->SetComputeRootDescriptorTable(TexturesParam, InScene->GetMaterialTexturesGPUDescriptor());
    }

    int VertexBufferParam = FindSrvRootParameterIndex(0, 2);
    if (VertexBufferParam >= 0)
    {
        InCommandList->SetComputeRootDescriptorTable(VertexBufferParam, InScene->GetVertexBuffersGPUDescriptor());
    }
}

bool CMaterial::CollectRootParameterCountsFromShaders(
    const std::vector<ID3DBlob*>& InShaderBlobs,
    UINT& OutCbvCount,
    UINT& OutSrvCount,
    UINT& OutUavCount,
    UINT& OutUnboundSrvCount,
    std::vector<UINT>& OutRootConstantsCountPerRegister)
{
    OutCbvCount = 0;
    OutSrvCount = 0;
    OutUavCount = 0;
    OutUnboundSrvCount = 0;
    OutRootConstantsCountPerRegister.clear();

    if (InShaderBlobs.empty())
    {
        LOG_ERROR("CollectRootParameterCountsFromShaders: Shader blob list is empty.");
        return false;
    }

    std::set<UINT> UnboundSrvSpaces;
    std::map<UINT, UINT> CbvConstantsPerRegister; 

    for (UINT BlobIdx = 0; BlobIdx < (UINT)InShaderBlobs.size(); ++BlobIdx)
    {
        ID3DBlob* ShaderBlob = InShaderBlobs[BlobIdx];
        if (ShaderBlob == nullptr || ShaderBlob->GetBufferPointer() == nullptr || ShaderBlob->GetBufferSize() == 0)
        {
            LOG_ERROR("CollectRootParameterCountsFromShaders: Invalid shader blob at index %u.", BlobIdx);
            return false;
        }

        DxcBuffer ShaderBuffer = {};
        ShaderBuffer.Ptr = ShaderBlob->GetBufferPointer();
        ShaderBuffer.Size = ShaderBlob->GetBufferSize();
        ShaderBuffer.Encoding = DXC_CP_ACP;

        ComPtr<ID3D12ShaderReflection> ShaderReflection;
        HRESULT HrShaderReflect = CRenderer::GetInstance().DxcUtils->CreateReflection(&ShaderBuffer, IID_PPV_ARGS(&ShaderReflection));
        if (SUCCEEDED(HrShaderReflect) && ShaderReflection != nullptr)
        {
            D3D12_SHADER_DESC ShaderDesc = {};
            HRESULT HrShaderDesc = ShaderReflection->GetDesc(&ShaderDesc);
            if (FAILED(HrShaderDesc))
            {
                LOG_ERROR("CollectRootParameterCountsFromShaders: GetDesc failed for shader blob %u (0x%08X).",
                    BlobIdx, HrShaderDesc);
                return false;
            }

            for (UINT ResourceIdx = 0; ResourceIdx < ShaderDesc.BoundResources; ++ResourceIdx)
            {
                D3D12_SHADER_INPUT_BIND_DESC BindingDesc = {};
                HRESULT HrBindingDesc = ShaderReflection->GetResourceBindingDesc(ResourceIdx, &BindingDesc);
                if (FAILED(HrBindingDesc))
                {
                    LOG_ERROR("CollectRootParameterCountsFromShaders: GetResourceBindingDesc failed (blob %u, resource %u, 0x%08X).",
                        BlobIdx, ResourceIdx, HrBindingDesc);
                    return false;
                }

                if (BindingDesc.Type == D3D_SIT_CBUFFER)
                {
                    if (BindingDesc.Space != 0)
                    {
                        LOG_ERROR("CBV '%s' uses space %u. Expected CBV space 0.", BindingDesc.Name, BindingDesc.Space);
                        return false;
                    }

                    ID3D12ShaderReflectionConstantBuffer* ConstBuffer =
                        ShaderReflection->GetConstantBufferByName(BindingDesc.Name);
                    if (ConstBuffer == nullptr)
                    {
                        LOG_ERROR("Failed to get constant buffer reflection for '%s'.", BindingDesc.Name);
                        return false;
                    }

                    UINT ConstantsCount = GetRootConstantsCount(BindingDesc.Name, ConstBuffer);

                    auto Iter = CbvConstantsPerRegister.find(BindingDesc.BindPoint);
                    if (Iter == CbvConstantsPerRegister.end() || ConstantsCount > Iter->second)
                    {
                        CbvConstantsPerRegister[BindingDesc.BindPoint] = ConstantsCount;
                    }

                    continue;
                }

                if (!AccumulateNonCbvBinding(BindingDesc, OutSrvCount, OutUavCount, UnboundSrvSpaces))
                {
                    return false;
                }
            }

            continue;
        }

        ComPtr<ID3D12LibraryReflection> LibraryReflection;
        HRESULT HrLibraryReflect = CRenderer::GetInstance().DxcUtils->CreateReflection(&ShaderBuffer, IID_PPV_ARGS(&LibraryReflection));
        if (FAILED(HrLibraryReflect) || LibraryReflection == nullptr)
        {
            LOG_ERROR("CollectRootParameterCountsFromShaders: Reflection failed for blob %u.", BlobIdx);
            return false;
        }

        D3D12_LIBRARY_DESC LibraryDesc = {};
        HRESULT HrLibraryDesc = LibraryReflection->GetDesc(&LibraryDesc);
        if (FAILED(HrLibraryDesc))
        {
            LOG_ERROR("CollectRootParameterCountsFromShaders: Library GetDesc failed for blob %u (0x%08X).",
                BlobIdx, HrLibraryDesc);
            return false;
        }

        for (UINT FunctionIdx = 0; FunctionIdx < LibraryDesc.FunctionCount; ++FunctionIdx)
        {
            ID3D12FunctionReflection* FunctionReflection = LibraryReflection->GetFunctionByIndex((INT)FunctionIdx);
            if (FunctionReflection == nullptr)
            {
                LOG_ERROR("GetFunctionByIndex failed (blob %u, function %u).", BlobIdx, FunctionIdx);
                return false;
            }

            D3D12_FUNCTION_DESC FunctionDesc = {};
            HRESULT HrFunctionDesc = FunctionReflection->GetDesc(&FunctionDesc);
            if (FAILED(HrFunctionDesc))
            {
                LOG_ERROR("Function GetDesc failed (blob %u, function %u, 0x%08X).",
                    BlobIdx, FunctionIdx, HrFunctionDesc);
                return false;
            }

            for (UINT ResourceIdx = 0; ResourceIdx < FunctionDesc.BoundResources; ++ResourceIdx)
            {
                D3D12_SHADER_INPUT_BIND_DESC BindingDesc = {};
                HRESULT HrBindingDesc = FunctionReflection->GetResourceBindingDesc(ResourceIdx, &BindingDesc);
                if (FAILED(HrBindingDesc))
                {
                    LOG_ERROR("Function GetResourceBindingDesc failed (blob %u, function %u, resource %u, 0x%08X).",
                        BlobIdx, FunctionIdx, ResourceIdx, HrBindingDesc);
                    return false;
                }

                if (BindingDesc.Type == D3D_SIT_CBUFFER)
                {
                    if (BindingDesc.Space != 0)
                    {
                        LOG_ERROR("CBV '%s' uses space %u. Expected CBV space 0.", BindingDesc.Name, BindingDesc.Space);
                        return false;
                    }

                    ID3D12ShaderReflectionConstantBuffer* ConstBuffer =
                        FunctionReflection->GetConstantBufferByName(BindingDesc.Name);
                    if (ConstBuffer == nullptr)
                    {
                        LOG_ERROR("Failed to get function constant buffer reflection for '%s'.", BindingDesc.Name);
                        return false;
                    }

                    UINT ConstantsCount = GetRootConstantsCount(BindingDesc.Name, ConstBuffer);

                    auto Iter = CbvConstantsPerRegister.find(BindingDesc.BindPoint);
                    if (Iter == CbvConstantsPerRegister.end() || ConstantsCount > Iter->second)
                    {
                        CbvConstantsPerRegister[BindingDesc.BindPoint] = ConstantsCount;
                    }

                    continue;
                }

                if (!AccumulateNonCbvBinding(BindingDesc, OutSrvCount, OutUavCount, UnboundSrvSpaces))
                {
                    return false;
                }
            }
        }
    }

    if (!UnboundSrvSpaces.empty())
    {
        OutUnboundSrvCount = *UnboundSrvSpaces.rbegin();
    }

    UINT MaxCbvRegister = 0;
    bool bHasCbvRegister = false;
    for (const auto& Pair : CbvConstantsPerRegister)
    {
        UINT Register = Pair.first;
        UINT ConstantsCount = Pair.second;

        if (ConstantsCount == 0)
        {
            ++OutCbvCount;
        }

        if (!bHasCbvRegister || Register > MaxCbvRegister)
        {
            MaxCbvRegister = Register;
            bHasCbvRegister = true;
        }
    }

    if (bHasCbvRegister)
    {
        for (UINT Register = 0; Register <= MaxCbvRegister; ++Register)
        {
            UINT ConstantsCount = 0;
            auto Iter = CbvConstantsPerRegister.find(Register);
            if (Iter != CbvConstantsPerRegister.end() && Iter->second > 0)
            {
                ConstantsCount = Iter->second;
            }

            OutRootConstantsCountPerRegister.push_back(ConstantsCount);
        }
    }

    return true;
}

ComPtr<ID3DBlob> CMaterial::ReadShaderFile(LPCWSTR InFileName)
{
    ComPtr<ID3DBlob> ShaderBlob;
    std::filesystem::path ExeDirectory = CRenderer::GetExeDirectory();
    HRESULT Hr = D3DReadFileToBlob((ExeDirectory / InFileName).c_str(), &ShaderBlob);
    if (FAILED(Hr))
    {
		LOG_ERROR("ReadShaderFile: Failed to read shader file '%ls' (0x%08X).", InFileName, Hr);
		return nullptr;
    }

	return ShaderBlob;
}