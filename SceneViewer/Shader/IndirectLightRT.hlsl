#include "RaytracingCommon.hlsli"

Texture2D GBufferB : register(t2);
Texture2D DepthBuffer : register(t3);

RWTexture2D<float4> OutTexture : register(u0);

struct ShadowPayload
{
    float Shadow;
};

struct IndirectPayload
{
    float3 Color;
};

[shader("raygeneration")]
void IndirectRayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 Dimensions = DispatchRaysDimensions();
    float2 ScreenUv = float2(launchIndex.x / (float) Dimensions.x, launchIndex.y / (float) Dimensions.y);
    
    float4 Normal = GBufferB.Load(launchIndex);
    
    float3 N = Normal.xyz * 2.0f - 1.0f;
    if (length(N) < 0.01f)
    {
        OutTexture[launchIndex.xy] = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }
    N = normalize(N);

    float Depth = DepthBuffer.Load(launchIndex).r;
    float3 WldPos = GetWorldPositionFromDepth(Depth, ScreenUv).xyz + N * 1.5f;
    
    uint RandSeed = initRand(launchIndex.x + launchIndex.y * Dimensions.x, FrameNumber);
    
    uint SampleCount = 2;
    float3 FinalColor = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < SampleCount; ++i)
    {
        float Rand1 = PCG_rand(RandSeed);
        float Rand2 = PCG_rand(RandSeed);
        float3 SampleDir = hemisphereSample_cos(Rand1, Rand2);
        
        float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
        float3 T = normalize(cross(up, N));
        float3 B = cross(N, T);

        float3 WldSampleDir = normalize(SampleDir.z * N + SampleDir.x * T + SampleDir.y * B);
    
        RayDesc Ray;
        Ray.Origin = WldPos;
        Ray.Direction = WldSampleDir;

        Ray.TMin = 1.5f;
        Ray.TMax = 5000;

        IndirectPayload Payload;
        TraceRay(RtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, Ray, Payload);
        FinalColor += Payload.Color;
    }

    //float3 PreColor = OutTexture[launchIndex.xy].rgb;
    //float3 BlendColor = lerp(PreColor, FinalColor.rgb / (float) SampleCount, 0.05f);
    //OutTexture[launchIndex.xy] = float4(BlendColor, 1.0f);
    OutTexture[launchIndex.xy] = float4(FinalColor.rgb / (float) SampleCount, 1.0f);
}

[shader("miss")]
void IndirectMiss(inout IndirectPayload Payload)
{
    Payload.Color = float3(0.0f, 0.0f, 0.0f);
}

[shader("closesthit")]
void IndirectClosestHit(inout IndirectPayload Payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint MeshIdx = InstanceID();
    SHitVertexAttributes HitVertex = GetHitVertexAttributes(attribs.barycentrics);

    int TexIdx = AllMeshes[MeshIdx].TextureIdx;
    Texture2D DiffuseTexture = MaterialTextures[TexIdx * 2];
    float3 Albedo = DiffuseTexture.SampleLevel(AnisotropicSampler, HitVertex.Uv, 0).rgb;
    float3 N = HitVertex.Normal;
    
    float3 WldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    WldPos += N * 1.5f;
    RayDesc Ray;
    Ray.Origin = WldPos;
    Ray.Direction = DirectionalLight.xyz;

    Ray.TMin = 1.5f;
    Ray.TMax = 5000;
    
    ShadowPayload ShadowRes;
    TraceRay(RtScene, 0 /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, Ray, ShadowRes);
    
    float3 L = DirectionalLight.xyz;
    Payload.Color = Albedo * max(dot(N, L), 0.0f) * ShadowRes.Shadow * DirectionalLight.w; //
}

[shader("anyhit")]
void IndirectAnyHit(inout IndirectPayload Payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint MeshIdx = InstanceID();
    SHitVertexAttributes HitVertex = GetHitVertexAttributes(attribs.barycentrics);

    int TexIdx = AllMeshes[MeshIdx].TextureIdx;
    Texture2D DiffuseTexture = MaterialTextures[TexIdx * 2];
    
    float Alpha = DiffuseTexture.SampleLevel(AnisotropicSampler, HitVertex.Uv, 0).a;
    
    if (Alpha < 0.5f)
    {
        IgnoreHit();
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
    uint MeshIdx = InstanceID();
    SHitVertexAttributes HitVertex = GetHitVertexAttributes(attribs.barycentrics);

    int TexIdx = AllMeshes[MeshIdx].TextureIdx;
    Texture2D DiffuseTexture = MaterialTextures[TexIdx * 2];
    
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
