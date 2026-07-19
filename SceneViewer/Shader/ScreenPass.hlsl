#include "Common.hlsli"

QuadVS_Output VSMain(VS_INPUT Input)
{
    QuadVS_Output Output;
    Output.Pos = Input.Position;
    Output.Uv = Input.Texcoord;
    return Output;
}

Texture2D GBufferA : register(t0);
Texture2D GBufferB : register(t1);
Texture2D DepthBuffer : register(t2);

Texture2D ShadowRT : register(t3);
Texture2D IndirectLightRT : register(t4);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState AnisotropicSampler : register(s2);

float4 PSLighting(QuadVS_Output Input) : SV_TARGET
{    
    float4 Albedo = GBufferA.Sample(PointSampler, Input.Uv);
    float4 Normal = GBufferB.Sample(PointSampler, Input.Uv);

    float Depth = DepthBuffer.Sample(PointSampler, Input.Uv).r;

    float3 N = Normal.xyz * 2.0f - 1.0f;
    if (length(N) < 0.01f)
    {
        return float4(0.529f, 0.808f, 0.922f, 1.0f);
    }
    N = normalize(N);

    float4 WldPos = GetWorldPositionFromDepth(Depth, Input.Uv);

    float3 L = DirectionalLight.xyz;
    float3 V = normalize(CameraOrigin.xyz - WldPos.xyz);

    float roughness = Albedo.a;
    float metal = Normal.a;

    float Shadow = ShadowRT.Sample(LinearSampler, Input.Uv).r;

    float3 IndirectLighting = IndirectLightRT.Sample(LinearSampler, Input.Uv).rgb;
    float3 Color = CalculatePBR(L, N, V, roughness, metal, Albedo.rgb, DirectionalLight.w) * Shadow + Albedo.rgb * IndirectLighting;
    
    // test indirect lihghting
    //float3 Color = IndirectLighting;
    
    Color.rgb = ACESFitted(Color.rgb);

    return float4(Color, 1.0f);
//	float Z = mProjection._m32 / (Color.r - mProjection._m22);
}

