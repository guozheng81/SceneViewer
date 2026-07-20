#include "Common.hlsli"

Texture2D<float3> CurrentLighting : register(t0);
Texture2D<float4> HistoryLighting : register(t1);
Texture2D<float> DepthBuffer : register(t2);
Texture2D HistoryDepthBuffer : register(t3);

RWTexture2D<float4> AccumulatedLighting : register(u0);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState AnisotropicSampler : register(s2);

float GetLinearZ(float InDepth)
{
    return Proj_m32 / (InDepth - Proj_m22);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 texCoord = int2(dispatchThreadID.xy);

    float3 currentLighting = CurrentLighting[texCoord];


    float centerDepth = DepthBuffer[texCoord];
    float2 ScreenUv = float2(texCoord.x / ViewportSize.x, texCoord.y / ViewportSize.y);
    float4 WldPos = GetWorldPositionFromDepth(centerDepth, ScreenUv);

    float4 PrevProjPos = mul(WldPos, mPrevViewProjection);
    PrevProjPos.xyz /= PrevProjPos.w;

    float2 historyUV = PrevProjPos.xy;
    historyUV.x = (historyUV.x + 1.0f) * 0.5f;
    historyUV.y = (1.0f - historyUV.y) * 0.5f;
    historyUV += ViewportSize.zw * 0.5f;

    float expectedDepth = GetLinearZ(PrevProjPos.z);
    float historyDepth = GetLinearZ(HistoryDepthBuffer.SampleLevel(PointSampler, historyUV, 0).r);
    
    if (any(historyUV < 0.0f) || any(historyUV > 1.0f) || abs(expectedDepth - historyDepth) > expectedDepth * 0.02f)
    {
        AccumulatedLighting[texCoord] = float4(currentLighting, 1.0f);
        return;
    }

    float4 historyData = HistoryLighting.SampleLevel(LinearSampler, historyUV, 0);
    float3 historyColor = historyData.rgb;
    float historyLength = historyData.a;
    
    float maxHistoryLength = 32.0f;
    float newHistoryLength = min(historyLength + 1.0f, maxHistoryLength);

    float alpha = 1.0f / newHistoryLength;
    float3 accumulatedColor = lerp(historyColor, currentLighting, alpha);

    AccumulatedLighting[texCoord] = float4(accumulatedColor, newHistoryLength);
}