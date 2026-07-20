#include "Common.hlsli"

Texture2D<float3> CurrentLighting : register(t0);
Texture2D<float3> HistoryLighting : register(t1);
Texture2D<float> DepthBuffer : register(t2);

RWTexture2D<float3> AccumulatedLighting : register(u0);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState AnisotropicSampler : register(s2);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 texCoord = int2(dispatchThreadID.xy);

    float3 currentLighting = CurrentLighting[texCoord];


    float centerDepth = DepthBuffer[texCoord];
    float2 ScreenUv = float2(texCoord.x / ViewportSize.x, texCoord.y / ViewportSize.y);
    float4 WldPos = GetWorldPositionFromDepth(centerDepth, ScreenUv);

    float4 PrevProjPos = mul(WldPos, mPrevViewProjection);

    float2 historyUV = PrevProjPos.xy/PrevProjPos.w;
    historyUV.x = (historyUV.x + 1.0f) * 0.5f;
    historyUV.y = (1.0f - historyUV.y) * 0.5f;
    historyUV += ViewportSize.zw * 0.5f;

    if (any(historyUV < 0.0f) || any(historyUV > 1.0f))
    {
        AccumulatedLighting[texCoord] = currentLighting;
        return;
    }

    float3 historyColor = HistoryLighting.SampleLevel(LinearSampler, historyUV, 0);
    

    float alpha = 0.05f;
    float3 accumulatedColor = lerp(historyColor, currentLighting, alpha);

    AccumulatedLighting[texCoord] = accumulatedColor;
}