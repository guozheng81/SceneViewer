#include "Common.hlsli"

RaytracingAccelerationStructure RtScene : register(t0);
RWTexture2D<float4> OutTexture : register(u0);

[shader("raygeneration")]
void PrimaryRayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    OutTexture[launchIndex.xy] = float4(1.0f, 1.0f, 0.0f, 1.0f);
}

struct Payload
{
    bool hit;
};

[shader("miss")]
void PrimaryMiss(inout Payload payload)
{
    payload.hit = false;
}

[shader("closesthit")]
void PrimaryClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.hit = true;
}
