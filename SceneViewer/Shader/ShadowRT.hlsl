#include "RaytracingCommon.hlsli"

Texture2D GBufferB : register(t2);
Texture2D DepthBuffer : register(t3);

RWTexture2D<float> OutTexture : register(u0);

[shader("raygeneration")]
void ShadowRayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 Dimensions = DispatchRaysDimensions();
    float2 ScreenUv = float2(launchIndex.x / (float) Dimensions.x, launchIndex.y / (float) Dimensions.y);
    
    float4 Normal = GBufferB.Load(launchIndex);
    
    float3 N = Normal.xyz * 2.0f - 1.0f;
    if (length(N) < 0.01f)
    {
        OutTexture[launchIndex.xy].r = 1.0f;
        return;
    }
    N = normalize(N);

    float Depth = DepthBuffer.Load(launchIndex).r;
    float3 WldPos = GetWorldPositionFromDepth(Depth, ScreenUv).xyz + N*1.5f;
    
    RayDesc Ray;
    Ray.Origin = WldPos;
    Ray.Direction = DirectionalLight.xyz;

    Ray.TMin = 1.5f;
    Ray.TMax = 5000;

    ShadowPayload payload;
    TraceRay(RtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, Ray, payload);

    OutTexture[launchIndex.xy].r = payload.Shadow;
}

