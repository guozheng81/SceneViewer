#include "RaytracingCommon.hlsli"

// SH L1 irradiance volume: 4 coefficients (1 DC + 3 linear) packed as RGBA, one texture per color channel.
RWTexture3D<float4> SHVolumeR : register(u0);
RWTexture3D<float4> SHVolumeG : register(u1);
RWTexture3D<float4> SHVolumeB : register(u2);

cbuffer cbIrradianceVolume : register(b1)
{
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

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

[shader("raygeneration")]
void IrradianceVolumeRayGen()
{
    uint3 ProbeIdx = DispatchRaysIndex();
    uint3 VolumeResolution = DispatchRaysDimensions();

    float3 ProbePos = BoundingBoxMin.xyz + ((float3) ProbeIdx + 0.5f) * VolumeCellSize;

    float4 SHR = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 SHG = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 SHB = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    const uint SamplesPerProbe = 160;
    for (uint i = 0; i < SamplesPerProbe; ++i)
    {
        uint sampleIndex = (i + FrameNumber * SamplesPerProbe) % 1024;
        float2 xi = Hammersley(sampleIndex, 1024);

        // Uniform sample over the full sphere since a probe gathers radiance from all directions.
        float Phi = xi.y * 2.0f * PI;
        float CosTheta = 1.0f - 2.0f * xi.x;
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

    float4 PrevSHR = SHVolumeR[ProbeIdx];
    float4 PrevSHG = SHVolumeG[ProbeIdx];
    float4 PrevSHB = SHVolumeB[ProbeIdx];
    
    //float BlendFactor = (FrameNumber == 0) ? 1.0f : (1.0f / (float) (FrameNumber + 1));
    float BlendFactor = 0.01f;
    SHVolumeR[ProbeIdx] = lerp(PrevSHR, SHR, BlendFactor);
    SHVolumeG[ProbeIdx] = lerp(PrevSHG, SHG, BlendFactor);
    SHVolumeB[ProbeIdx] = lerp(PrevSHB, SHB, BlendFactor);
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
    Payload.Color = Albedo / 3.14159265f * max(dot(N, L), 0.0f) * ShadowRes.Shadow * DirectionalLight.w;
}

[shader("anyhit")]
void IrradianceVolumeAnyHit(inout IrradiancePayload Payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint InstanceIdx = InstanceID();
    SHitVertexAttributes HitVertex = GetHitVertexAttributes(attribs.barycentrics);

    int TexIdx = AllMeshes[InstanceIdx].AlbedoTextureIdx;
    if (TexIdx >= 0)
    {
        Texture2D DiffuseTexture = MaterialTextures[TexIdx];
    
        float Alpha = DiffuseTexture.SampleLevel(AnisotropicSampler, HitVertex.Uv, 0).a;
    
        if (Alpha < 0.5f)
        {
            IgnoreHit();
        }
    }
}


[shader("miss")]
void ShadowMiss(inout ShadowPayload payload)
{
    payload.Shadow = 1.0f;
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.Shadow = 0.025f;
}

[shader("anyhit")]
void ShadowAnyHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint InstanceIdx = InstanceID();
    SHitVertexAttributes HitVertex = GetHitVertexAttributes(attribs.barycentrics);

    int TexIdx = AllMeshes[InstanceIdx].AlbedoTextureIdx;
    if (TexIdx < 0)
    {
        payload.Shadow = 0.025f;
        AcceptHitAndEndSearch();
    }
    
    Texture2D DiffuseTexture = MaterialTextures[TexIdx];
    
    float Alpha = DiffuseTexture.SampleLevel(AnisotropicSampler, HitVertex.Uv, 0).a;
    
    if (Alpha < 0.5f)
    {
        IgnoreHit();
    }
    else
    {
        payload.Shadow = 0.025f;
        AcceptHitAndEndSearch();

    }
}
