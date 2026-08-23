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
    uint InstanceIdx = InstanceID();
    uint MeshIdx = AllMeshes[InstanceIdx].MeshIdx;
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

uint initRand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
    for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

float nextRand(inout uint s)
{
    s = (1664525u * s + 1013904223u);
    return float(s & 0x00FFFFFF) / float(0x01000000);
}

float PCG_rand(inout uint seed)
{
    // LCG step to advance the internal state
    seed = seed * 747796405u + 2891336453u;
    
    // PCG permutation step
    uint word = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
    uint result = (word >> 22u) ^ word;
    
    // Map the 32-bit integer to a float range [0.0, 1.0)
    return float(result) / 4294967296.0;
}

float3 hemisphereSample_cos(float u, float v)
{
    float phi = v * 2.0f * 3.1415926f;
    float cosTheta = sqrt(1.0f - u);
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}
