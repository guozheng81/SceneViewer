#include "Common.hlsli"

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState LinearSamplerWrap : register(s2);

Texture2D DiffuseTexture : register(t0);

struct VS_INPUT
{
    float4 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Texcoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float3 Normal : NORMAL;
    float2 Texcoord : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
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
    Output.Texcoord = Input.Texcoord;
    Output.WorldPos = float4(Input.Position.xyz, 1.0f);
	
    return Output;
}

struct PS_INPUT
{
    float3 Normal : NORMAL;
    float2 Texcoord : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
};

float4 PSMain(PS_INPUT Input) : SV_TARGET
{
    float4 Albedo = DiffuseTexture.Sample(LinearSamplerWrap, Input.Texcoord);

    if (Albedo.a < 0.5f)
    {
        discard;
    }
    
    float4 WldPos = Input.WorldPos;

    float3 N = normalize(Input.Normal);
    float3 L = normalize(float3(0.5f, 1.0f, 0.5f));
    float3 V = normalize(CameraOrigin.xyz - WldPos.xyz);

    float roughness = 0.7f;
    float metal = 0.25f;

    float4 FinalColor;
    FinalColor.rgb = CalculatePBR(L, N, V, roughness, metal, Albedo.rgb, 4.0f) + Albedo.rgb*0.3f;
    FinalColor.a = 1.0f;
    
    return FinalColor;
}
