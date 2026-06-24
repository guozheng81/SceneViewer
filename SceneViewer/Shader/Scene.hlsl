#include "Common.hlsli"

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState LinearSamplerWrap : register(s2);

Texture2D DiffuseTexture : register(t0);

struct VS_INPUT
{
    float4 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float2 Texcoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float3 Normal : NORMAL;
    float2 Texcoord : TEXCOORD0;
    float3 Tangent : TEXCOORD1;
    float4 WorldPos : TEXCOORD2;
    float4 Position : SV_POSITION;
};

VS_OUTPUT VSMain(VS_INPUT Input)
{
    VS_OUTPUT Output;

    float4 LocalPos = Input.Position;
    LocalPos.w = 1.0f;
	
    float4x4 ViewProjMtx = mul(mView, mProjection);
    Output.Position = mul(LocalPos, ViewProjMtx);

    Output.Normal = Input.Normal;
    Output.Tangent = Input.Tangent;
    Output.Texcoord = Input.Texcoord;
    Output.WorldPos = float4(Input.Position.xyz, 1.0f);
	
    return Output;
}

struct PS_INPUT
{
    float3 Normal : NORMAL;
    float2 Texcoord : TEXCOORD0;
    float3 Tangent : TEXCOORD1;
    float4 WorldPos : TEXCOORD2;
};

float4 PSMain(PS_INPUT Input) : SV_TARGET
{
    float4 Albedo = DiffuseTexture.Sample(LinearSamplerWrap, Input.Texcoord);

    if (Albedo.a < 0.5f)
    {
        discard;
    }
    return Albedo;
}
