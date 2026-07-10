#include "Common.hlsli"

RaytracingAccelerationStructure RtScene : register(t0);
RWTexture2D<float4> OutTexture : register(u0);

struct Payload
{
    bool hit;
};

[shader("raygeneration")]
void PrimaryRayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    float2 ScreenUv = float2(launchIndex.x / 1280.0f, launchIndex.y / 720.0f);
    float4 WldPos = GetWorldPositionFromDepth(0.0f, ScreenUv);
    
    RayDesc Ray;
    Ray.Origin = CameraOrigin.xyz;
    Ray.Direction = normalize(WldPos.xyz - CameraOrigin.xyz);

    Ray.TMin = 0;
    Ray.TMax = 100000;

    Payload payload;
    TraceRay(RtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, Ray, payload);

    if (payload.hit)
    {
        OutTexture[launchIndex.xy] = float4(1.0f, 1.0f, 0.0f, 1.0f);        
    }
    else
    {
        OutTexture[launchIndex.xy] = float4((Ray.Direction + 1.0f) * 0.5f, 1.0f); //
//        float4(1.0f, 0.0f, 1.0f, 1.0f);
    }
}

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
