#include "RaytracingCommon.hlsli"

RWTexture2D<float4> OutTexture : register(u0);

struct Payload
{
    float3 Color;
};

[shader("raygeneration")]
void PrimaryRayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 Dimensions = DispatchRaysDimensions();
    float2 ScreenUv = float2(launchIndex.x / (float)Dimensions.x, launchIndex.y / (float)Dimensions.y);
    float4 WldPos = GetWorldPositionFromDepth(0.0f, ScreenUv);
    
    RayDesc Ray;
    Ray.Origin = CameraOrigin.xyz;
    Ray.Direction = normalize(WldPos.xyz - CameraOrigin.xyz);

    Ray.TMin = 0;
    Ray.TMax = 100000;

    Payload payload;
    TraceRay(RtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, Ray, payload);

    OutTexture[launchIndex.xy] = float4(payload.Color, 1.0f);
}

[shader("miss")]
void PrimaryMiss(inout Payload payload)
{
    payload.Color = float3(0.0f, 0.0f, 0.0f);
}

[shader("closesthit")]
void PrimaryClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint InstanceIdx = InstanceID();
    SHitVertexAttributes HitVertex = GetHitVertexAttributes(attribs.barycentrics);

    int TexIdx = AllMeshes[InstanceIdx].TextureIdx;
    Texture2D DiffuseTexture = MaterialTextures[TexIdx * 2];
    
    payload.Color = DiffuseTexture.SampleLevel(AnisotropicSampler, HitVertex.Uv, 0).rgb;
}

[shader("anyhit")]
void PrimaryAnyHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint InstanceIdx = InstanceID();
    SHitVertexAttributes HitVertex = GetHitVertexAttributes(attribs.barycentrics);

    int TexIdx = AllMeshes[InstanceIdx].TextureIdx;
    Texture2D DiffuseTexture = MaterialTextures[TexIdx * 2];
    
    float Alpha = DiffuseTexture.SampleLevel(AnisotropicSampler, HitVertex.Uv, 0).a;
    
    if(Alpha < 0.5f)
    {
        IgnoreHit();
    }
}
