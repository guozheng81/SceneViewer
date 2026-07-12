#include "Common.hlsli"

struct SVertex
{
    float3 Position;
    float3 Normal;
    float2 Uv;
};

StructuredBuffer<MeshInfo> AllMeshes : register(t0);
RaytracingAccelerationStructure RtScene : register(t1);
RWTexture2D<float4> OutTexture : register(u0);

Texture2D MaterialTextures[] : register(t0, space1);
StructuredBuffer<SVertex> VertexBufferArray[] : register(t0, space2);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState AnisotropicSampler : register(s2);

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
    uint MeshIdx = InstanceID();
    uint TriangleID = PrimitiveIndex();
    SVertex Vert0 = VertexBufferArray[MeshIdx][TriangleID * 3];
    SVertex Vert1 = VertexBufferArray[MeshIdx][TriangleID * 3 + 1];
    SVertex Vert2 = VertexBufferArray[MeshIdx][TriangleID * 3 + 2];
    
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y,
                                 attribs.barycentrics.x,
                                 attribs.barycentrics.y);

    float3 Normal = Vert0.Normal * barycentrics.x + Vert1.Normal * barycentrics.y + Vert2.Normal * barycentrics.z;
    Normal = normalize(Normal);
    
    float2 Uv = Vert0.Uv * barycentrics.x + Vert1.Uv * barycentrics.y + Vert2.Uv * barycentrics.z;

    int TexIdx = AllMeshes[MeshIdx].TextureIdx;
    Texture2D DiffuseTexture = MaterialTextures[TexIdx * 2];
    
    payload.Color = DiffuseTexture.SampleLevel(AnisotropicSampler, Uv, 0).rgb;
}
