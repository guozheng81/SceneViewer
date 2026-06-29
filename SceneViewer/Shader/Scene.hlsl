#include "Common.hlsli"

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState AnisotropicSampler : register(s2);

Texture2D DiffuseTexture : register(t0);
Texture2D NormalTexture : register(t1);

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
    float4 Albedo = DiffuseTexture.Sample(AnisotropicSampler, Input.Texcoord);

    if (Albedo.a < 0.5f)
    {
        discard;
    }
    
    float3 WldPos = Input.WorldPos.xyz;
    float3 WldNormal = normalize(Input.Normal);
    
    float3 PosDdx = ddx(WldPos);
    float3 PosDdy = ddy(WldPos);
    float2 UvDdx = ddx(Input.Texcoord);
    float2 UvDdy = ddy(Input.Texcoord);

    float3 Perp2 = cross(PosDdy, WldNormal);
    float3 Perp1 = cross(WldNormal, PosDdx);
    float3 T = Perp2 * UvDdx.x + Perp1 * UvDdy.x;
    float3 B = Perp2 * UvDdx.y + Perp1 * UvDdy.y;
    
    float Invmax = rsqrt(max(dot(T, T), dot(B, B)));
    T *= Invmax;
    B *= Invmax;
    
    float3 NormalColor = NormalTexture.Sample(AnisotropicSampler, Input.Texcoord).rgb;
    NormalColor = (NormalColor * 2.0f - 1.0f);

    float3 N = T * NormalColor.x + B * NormalColor.y + WldNormal * NormalColor.z;
    N = normalize(N);    

    float3 L = DirectionalLight.xyz;
    float3 V = normalize(CameraOrigin.xyz - WldPos);

    float roughness = 0.7f;
    float metal = 0.25f;

    float4 FinalColor;
    FinalColor.rgb = CalculatePBR(L, N, V, roughness, metal, Albedo.rgb, DirectionalLight.w) + Albedo.rgb * 0.3f;
    //FinalColor.rgb = (N + 1.0f) * 0.5f;
    FinalColor.a = 1.0f;
    
    return FinalColor;
}
