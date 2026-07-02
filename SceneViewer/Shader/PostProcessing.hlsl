#include "Common.hlsli"

QuadVS_Output VSMain(VS_INPUT Input)
{
    QuadVS_Output Output;
    Output.Pos = Input.Position;
    Output.Uv = Input.Texcoord;
    return Output;
}

Texture2D<float4> SceneColor : register(t0);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState AnisotropicSampler : register(s2);

float4 PSMain(QuadVS_Output Input) : SV_TARGET
{
    float4 Color = SceneColor.Sample(LinearSampler, Input.Uv);
    Color.rgb = ACESFitted(Color.rgb);

    return Color;
//	float Z = mProjection._m32 / (Color.r - mProjection._m22);
}

