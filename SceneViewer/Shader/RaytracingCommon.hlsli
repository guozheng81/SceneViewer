#include "Common.hlsli"

struct SVertex
{
    float3 Position;
    float3 Normal;
    float2 Uv;
};

StructuredBuffer<MeshInfo> AllMeshes : register(t0);
RaytracingAccelerationStructure RtScene : register(t1);

Texture2D MaterialTextures[] : register(t0, space1);
StructuredBuffer<SVertex> VertexBufferArray[] : register(t0, space2);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState AnisotropicSampler : register(s2);

struct SHitVertexAttributes
{
    float3 Normal;
    float2 Uv;    
};

SHitVertexAttributes GetHitVertexAttributes(float2 BarycentricXY)
{
    uint MeshIdx = InstanceID();
    uint TriangleID = PrimitiveIndex();
    SVertex Vert0 = VertexBufferArray[MeshIdx][TriangleID * 3];
    SVertex Vert1 = VertexBufferArray[MeshIdx][TriangleID * 3 + 1];
    SVertex Vert2 = VertexBufferArray[MeshIdx][TriangleID * 3 + 2];
    
    float3 Barycentrics = float3(1.0 - BarycentricXY.x - BarycentricXY.y, BarycentricXY.x, BarycentricXY.y);

    SHitVertexAttributes Result;
    float3 Normal = Vert0.Normal * Barycentrics.x + Vert1.Normal * Barycentrics.y + Vert2.Normal * Barycentrics.z;
    Result.Normal = normalize(Normal);
    
    Result.Uv = Vert0.Uv * Barycentrics.x + Vert1.Uv * Barycentrics.y + Vert2.Uv * Barycentrics.z;

    return Result;
}