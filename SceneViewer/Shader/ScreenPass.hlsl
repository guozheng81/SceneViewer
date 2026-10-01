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

Texture3D SHVolumeR : register(t5);
Texture3D SHVolumeG : register(t6);
Texture3D SHVolumeB : register(t7);

float3 DebugDrawIrradianceProbes(Texture3D InSHVolumeR, Texture3D InSHVolumeG, Texture3D InSHVolumeB,
                                 float3 RayOrigin, float3 RayDir, float MaxT, float3 SceneColor)
{
    uint3 VolumeResolution;
    InSHVolumeR.GetDimensions(VolumeResolution.x, VolumeResolution.y, VolumeResolution.z);

    float3 VolumeCellSize = BoundingBoxSize.xyz / (VolumeResolution - 1);
    float MinCellSize = min(VolumeCellSize.x, min(VolumeCellSize.y, VolumeCellSize.z));
    float ProbeRadius = MinCellSize * 0.08f;
    float StepSize = MinCellSize * 0.5f;

    float BestT = MaxT;
    int3 BestCoords = int3(-1, -1, -1);
    float3 BestProbePos = float3(0.0f, 0.0f, 0.0f);

    [loop]
    for (float T = 0.0f; T < MaxT && T <= BestT; T += StepSize)
    {
        float3 SamplePos = RayOrigin + RayDir * T;
        int3 ProbeCoords = int3(round((SamplePos - BoundingBoxMin.xyz) / VolumeCellSize));

        if (any(ProbeCoords < 0) || any(ProbeCoords >= int3(VolumeResolution)))
        {
            continue;
        }

        float3 ProbePos = BoundingBoxMin.xyz + float3(ProbeCoords) * VolumeCellSize;
        float3 OC = RayOrigin - ProbePos;
        float B = dot(OC, RayDir);
        float C = dot(OC, OC) - ProbeRadius * ProbeRadius;
        float Disc = B * B - C;

        if (Disc > 0.0f)
        {
            float HitT = -B - sqrt(Disc);
            if (HitT > 0.0f && HitT < BestT)
            {
                BestT = HitT;
                BestCoords = ProbeCoords;
                BestProbePos = ProbePos;
            }
        }
    }

    if (BestCoords.x < 0)
    {
        return SceneColor;
    }

    float3 HitPos = RayOrigin + RayDir * BestT;
    float3 SphereN = normalize(HitPos - BestProbePos);
    float4 SHTransfer = SHCosTransfer(SphereN);

    float4 SH_R = InSHVolumeR.Load(int4(BestCoords, 0));
    float4 SH_G = InSHVolumeG.Load(int4(BestCoords, 0));
    float4 SH_B = InSHVolumeB.Load(int4(BestCoords, 0));

    float3 Irradiance = float3(dot(SH_R, SHTransfer), dot(SH_G, SHTransfer), dot(SH_B, SHTransfer));
    return max(Irradiance, 0.0f) / PI;
}

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

    float3 L = DirectionalLight.xyz;
    float3 V = normalize(CameraOrigin.xyz - WldPos.xyz);

    float roughness = Albedo.a;
    float metal = Normal.a;

    float Shadow = ShadowRT.Sample(LinearSampler, Input.Uv).r;

    float3 Color = CalculatePBR(L, N, V, roughness, metal, Albedo.rgb, DirectionalLight.w) * Shadow;
    
    [branch]
    if (UseIndirectLighting)
    {
        float3 IndirectLighting = IndirectLightRT.Sample(LinearSampler, Input.Uv).rgb;
        Color += IndirectLighting * Albedo.rgb;
    }
    else if (UseIrradianceVolume)
    {
        float3 IrradianceLight = SampleIrradiance(SHVolumeR, SHVolumeG, SHVolumeB, WldPos.xyz, N);
        Color += IrradianceLight * Albedo.rgb;
    }
    
    /*
    // debug draw irradiance probes
    float3 RayDir = normalize(WldPos.xyz - CameraOrigin.xyz);
    float MaxT = length(WldPos.xyz - CameraOrigin.xyz);
    Color = DebugDrawIrradianceProbes(SHVolumeR, SHVolumeG, SHVolumeB, CameraOrigin.xyz, RayDir, MaxT, Color);
    */
        
    Color.rgb = ACESFitted(Color.rgb);

    return float4(Color, 1.0f);
//	float Z = mProjection._m32 / (Color.r - mProjection._m22);
}

