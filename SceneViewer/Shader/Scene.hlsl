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
	
    Output.Position = LocalPos;

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
    return float4(1.0f, 1.0f, 0.0f, 1.0f);
}
