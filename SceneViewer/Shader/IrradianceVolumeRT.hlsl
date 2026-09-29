#include "RaytracingCommon.hlsli"

// SH L1 irradiance volume: 4 coefficients (1 DC + 3 linear) packed as RGBA, one texture per color channel.
RWTexture3D<float4> SHVolumeR : register(u0);
RWTexture3D<float4> SHVolumeG : register(u1);
RWTexture3D<float4> SHVolumeB : register(u2);

struct IrradiancePayload
{
    float3 Color;
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

static const uint Sobol_Vectors_D0[32] =
{
    0x80000000u, 0x40000000u, 0x20000000u, 0x10000000u,
    0x08000000u, 0x04000000u, 0x02000000u, 0x01000000u,
    0x00800000u, 0x00400000u, 0x00200000u, 0x00100000u,
    0x00080000u, 0x00040000u, 0x00020000u, 0x00010000u,
    0x00008000u, 0x00004000u, 0x00002000u, 0x00001000u,
    0x00000800u, 0x00000400u, 0x00000200u, 0x00000100u,
    0x00000080u, 0x00000040u, 0x00000020u, 0x00000010u,
    0x00000008u, 0x00000004u, 0x00000002u, 0x00000001u
};

static const uint Sobol_Vectors_D1[32] =
{
    0x80000000u, 0xc0000000u, 0xa0000000u, 0xf0000000u,
    0x88000000u, 0xcc000000u, 0xaa000000u, 0xff000000u,
    0x80800000u, 0xc0c00000u, 0xa0a00000u, 0xf0f00000u,
    0x88880000u, 0xcccc0000u, 0xaaaa0000u, 0xffff0000u,
    0x80008000u, 0xc000c000u, 0xa000a000u, 0xf000f000u,
    0x88008800u, 0xcc00cc00u, 0xaa00aa00u, 0xff00ff00u,
    0x80808080u, 0xc0c0c0c0u, 0xa0a0a0a0u, 0xf0f0f0f0u,
    0x88888888u, 0xccccccccu, 0xaaaaaaaau, 0xffffffffu
};


uint GetSobolSampleBits(uint SampleIndex, uint Vectors[32])
{
    uint GrayCode = SampleIndex ^ (SampleIndex >> 1);
    uint ResultBits = 0;

    [unroll]
    for (uint BitIdx = 0; BitIdx < 32; ++BitIdx)
    {
        if ((GrayCode & (1u << BitIdx)) != 0u)
        {
            ResultBits ^= Vectors[BitIdx];
        }
    }

    return ResultBits;
}

float GetSobolSampleScrambled(uint SampleIndex, uint Vectors[32], uint Scramble)
{
    uint Bits = GetSobolSampleBits(SampleIndex, Vectors);
    Bits ^= Scramble;
    return float(Bits) * (1.0f / 4294967296.0f);
}

float2 GetSobolPoint2DScrambled(uint SampleIndex, uint2 Scramble)
{
    float X = GetSobolSampleScrambled(SampleIndex, Sobol_Vectors_D0, Scramble.x);
    float Y = GetSobolSampleScrambled(SampleIndex, Sobol_Vectors_D1, Scramble.y);
    return float2(X, Y);
}

[shader("raygeneration")]
void IrradianceVolumeRayGen()
{
    uint3 ProbeIdx = DispatchRaysIndex();
    uint3 VolumeResolution = DispatchRaysDimensions();
    float3 VolumeCellSize = (BoundingBoxSize.xyz) / (VolumeResolution - 1);

    float3 ProbePos = BoundingBoxMin.xyz + ((float3) ProbeIdx) * VolumeCellSize;

    float4 SHR = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 SHG = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 SHB = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    uint ProbeLinearIdx =
    ProbeIdx.x +
    ProbeIdx.y * VolumeResolution.x +
    ProbeIdx.z * VolumeResolution.x * VolumeResolution.y;

    uint ScrambleX = initRand(ProbeLinearIdx, 0x12345678u);
    uint ScrambleY = initRand(ProbeLinearIdx, 0x87654321u);
        
    const uint SamplesPerProbe = 120;
    for (uint i = 0; i < SamplesPerProbe; ++i)
    {
        //uint sampleIndex = (i + FrameNumber * SamplesPerProbe) % 720;
        //float2 xi = Hammersley(sampleIndex, 720);
                
        uint SampleIndex = i + FrameNumber * SamplesPerProbe;
        float2 xi = GetSobolPoint2DScrambled(SampleIndex, uint2(ScrambleX, ScrambleY));

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
    float Weight = 4.0f*PI / (float) SamplesPerProbe;
    SHR *= Weight;
    SHG *= Weight;
    SHB *= Weight;

    float4 PrevSHR = SHVolumeR[ProbeIdx];
    float4 PrevSHG = SHVolumeG[ProbeIdx];
    float4 PrevSHB = SHVolumeB[ProbeIdx];
    
    float BlendFactor = (IrradianceFrameCount < 50 ? 0.02f : 1.0f / (IrradianceFrameCount));
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
    Payload.Color = Albedo * max(dot(N, L), 0.0f) * ShadowRes.Shadow * DirectionalLight.w;
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


