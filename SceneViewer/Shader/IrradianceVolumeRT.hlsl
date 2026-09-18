#include "RaytracingCommon.hlsli"

// SH L1 irradiance volume: 4 coefficients (1 DC + 3 linear) packed as RGBA, one texture per color channel.
RWTexture3D<float4> SHVolumeR : register(u0);
RWTexture3D<float4> SHVolumeG : register(u1);
RWTexture3D<float4> SHVolumeB : register(u2);

cbuffer cbIrradianceVolume : register(b1)
{
    float3 VolumeMin;
    float3 VolumeCellSize;
};

struct IrradiancePayload
{
    float3 Color;
};

struct ShadowPayload
{
    float Shadow;
};

[shader("raygeneration")]
void IrradianceVolumeRayGen()
{
    uint3 ProbeIdx = DispatchRaysIndex();
    uint3 VolumeResolution = DispatchRaysDimensions();

    float3 ProbePos = VolumeMin + (float3) ProbeIdx * VolumeCellSize;

    uint RandSeed = initRand(ProbeIdx.x + ProbeIdx.y * VolumeResolution.x + ProbeIdx.z * VolumeResolution.x * VolumeResolution.y, FrameNumber);

    float4 SHR = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 SHG = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 SHB = float4(0.0f, 0.0f, 0.0f, 0.0f);

    const uint SamplesPerProbe = 32;
    for (uint i = 0; i < SamplesPerProbe; ++i)
    {
        float Rand1 = PCG_rand(RandSeed);
        float Rand2 = PCG_rand(RandSeed);

        // Uniform sample over the full sphere since a probe gathers radiance from all directions.
        float Phi = Rand1 * 2.0f * PI;
        float CosTheta = 1.0f - 2.0f * Rand2;
        float SinTheta = sqrt(saturate(1.0f - CosTheta * CosTheta));
        float3 SampleDir = float3(cos(Phi) * SinTheta, sin(Phi) * SinTheta, CosTheta);

        RayDesc Ray;
        Ray.Origin = ProbePos;
        Ray.Direction = SampleDir;
        Ray.TMin = 0.01f;
        Ray.TMax = 5000;

        IrradiancePayload Payload;
        Payload.Color = float3(0.0f, 0.0f, 0.0f);
        TraceRay(RtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, Ray, Payload);

        float4 Basis = SHBasisFunc(SampleDir);

        SHR += Payload.Color.r * Basis;
        SHG += Payload.Color.g * Basis;
        SHB += Payload.Color.b * Basis;
    }

    // Monte-Carlo normalization for uniform sphere sampling: 4*PI / N.
    float Weight = 4.0f * PI / (float) SamplesPerProbe;
    SHR *= Weight;
    SHG *= Weight;
    SHB *= Weight;

    SHVolumeR[ProbeIdx] = SHR;
    SHVolumeG[ProbeIdx] = SHG;
    SHVolumeB[ProbeIdx] = SHB;
}

[shader("miss")]
void IrradianceVolumeMiss(inout IrradiancePayload Payload)
{
    Payload.Color = float3(0.0f, 0.0f, 0.0f);
}

[shader("closesthit")]
void IrradianceVolumeClosestHit(inout IrradiancePayload Payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint InstanceIdx = InstanceID();
    SHitVertexAttributes HitVertex = GetHitVertexAttributes(attribs.barycentrics);

    int TexIdx = AllMeshes[InstanceIdx].AlbedoTextureIdx;
    float3 Albedo = AllMeshes[InstanceIdx].Albedo;
    if (TexIdx >= 0)
    {
        Texture2D DiffuseTexture = MaterialTextures[TexIdx];
        Albedo = DiffuseTexture.SampleLevel(AnisotropicSampler, HitVertex.Uv, 0).rgb;
    }
    float3 N = HitVertex.Normal;

    float3 WldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    WldPos += N * 1.5f;

    RayDesc Ray;
    Ray.Origin = WldPos;
    Ray.Direction = DirectionalLight.xyz;

    Ray.TMin = 1.5f;
    Ray.TMax = 5000;

    ShadowPayload ShadowRes;
    ShadowRes.Shadow = 1.0f;
    TraceRay(RtScene, 0 /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, Ray, ShadowRes);

    float3 L = DirectionalLight.xyz;
    Payload.Color = Albedo * max(dot(N, L), 0.0f) * ShadowRes.Shadow * DirectionalLight.w;
}