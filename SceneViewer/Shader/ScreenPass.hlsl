#include "Common.hlsli"

QuadVS_Output VSMain(VS_INPUT Input)
{
    QuadVS_Output Output;
    Output.Pos = Input.Position;
    Output.Uv = Input.Texcoord;
    return Output;
}

Texture2D GBufferA : register(t0);
Texture2D GBufferB : register(t1);
Texture2D DepthBuffer : register(t2);

Texture2D ShadowRT : register(t3);
Texture2D IndirectLightRT : register(t4);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState AnisotropicSampler : register(s2);

#define _USE_IRRADIANCE_LIGHTING 0

#if _USE_IRRADIANCE_LIGHTING

Texture3D SHVolumeR : register(t5);
Texture3D SHVolumeG : register(t6);
Texture3D SHVolumeB : register(t7);

float3 SampleIrradiance(float3 WldPos, float3 N)
{
    float4 SHTransfer = SHCosTransfer(N);
    
    /*
    float3 Uvw = (WldPos - BoundingBoxMin.xyz) / BoundingBoxSize.xyz;
    float4 SH_R = SHVolumeR.SampleLevel(LinearSampler, Uvw, 0);
    float4 SH_G = SHVolumeG.SampleLevel(LinearSampler, Uvw, 0);
    float4 SH_B = SHVolumeB.SampleLevel(LinearSampler, Uvw, 0);
                    
    float3 ProbeLighting = float3(dot(SH_R, SHTransfer), dot(SH_G, SHTransfer), dot(SH_B, SHTransfer));
    return max(ProbeLighting, 0.0f);
    */
    
    uint3 VolumeResolution;
    SHVolumeR.GetDimensions(VolumeResolution.x, VolumeResolution.y, VolumeResolution.z);

    float3 VolumeCellSize = (BoundingBoxSize.xyz) / VolumeResolution;
    
    float3 BiasedWldPos = WldPos + N * 1.5f;
    
    float3 VoxelCoords = (BiasedWldPos - BoundingBoxMin.xyz) / VolumeCellSize - 0.5f;
    int3 BaseIndex = int3(floor(VoxelCoords));
    float3 FracWeights = frac(VoxelCoords);
    
    float3 TotalLighting = float3(0.0f, 0.0f, 0.0f);
    float TotalWeight = 0.0f;
    
    for (int z = 0; z <= 1; ++z)
    {
        for (int y = 0; y <= 1; ++y)
        {
            for (int x = 0; x <= 1; ++x)
            {
                int3 ProbeCoords = BaseIndex + int3(x, y, z);
                
                ProbeCoords = clamp(ProbeCoords, int3(0, 0, 0), int3(VolumeResolution) - 1);
                
                // Compute standard structural trilinear interpolation weight
                float3 TrilinearTerms = float3(x == 1 ? FracWeights.x : 1.0f - FracWeights.x,
                                               y == 1 ? FracWeights.y : 1.0f - FracWeights.y,
                                               z == 1 ? FracWeights.z : 1.0f - FracWeights.z);
                float CombinedWeight = TrilinearTerms.x * TrilinearTerms.y * TrilinearTerms.z;
                                                                
                if (CombinedWeight > 0.001f)
                {
                    float4 SH_R = SHVolumeR.Load(int4(ProbeCoords, 0));
                    float4 SH_G = SHVolumeG.Load(int4(ProbeCoords, 0));
                    float4 SH_B = SHVolumeB.Load(int4(ProbeCoords, 0));
                    
                    float3 ProbeLighting = float3(dot(SH_R, SHTransfer), dot(SH_G, SHTransfer), dot(SH_B, SHTransfer));
                    
                    TotalLighting += max(0.0f, ProbeLighting) * CombinedWeight;
                    TotalWeight += CombinedWeight;
                }
            }
        }
    }
    
    return TotalLighting / max(0.0001f, TotalWeight);
}

#endif

float4 PSLighting(QuadVS_Output Input) : SV_TARGET
{    
    float4 Albedo = GBufferA.Sample(PointSampler, Input.Uv);
    float4 Normal = GBufferB.Sample(PointSampler, Input.Uv);

    float Depth = DepthBuffer.Sample(PointSampler, Input.Uv).r;

    float3 N = Normal.xyz * 2.0f - 1.0f;
    if (length(N) < 0.01f)
    {
        return float4(0.529f, 0.808f, 0.922f, 1.0f);
    }
    N = normalize(N);

    float4 WldPos = GetWorldPositionFromDepth(Depth, Input.Uv);

    #if _USE_IRRADIANCE_LIGHTING
    // test irradiance lighting
    float3 IrradianceLight = SampleIrradiance(WldPos.xyz, N);
    return float4(ACESFitted(IrradianceLight * Albedo.rgb), 1.0f);
    #endif

    float3 L = DirectionalLight.xyz;
    float3 V = normalize(CameraOrigin.xyz - WldPos.xyz);

    float roughness = Albedo.a;
    float metal = Normal.a;

    float Shadow = ShadowRT.Sample(LinearSampler, Input.Uv).r;

    float3 IndirectLighting = IndirectLightRT.Sample(LinearSampler, Input.Uv).rgb;
    float3 Color = CalculatePBR(L, N, V, roughness, metal, Albedo.rgb, DirectionalLight.w) * Shadow + Albedo.rgb * IndirectLighting;
    
    // test indirect lihghting
    //float3 Color = IndirectLighting;
    
    Color.rgb = ACESFitted(Color.rgb);

    return float4(Color, 1.0f);
//	float Z = mProjection._m32 / (Color.r - mProjection._m22);
}

