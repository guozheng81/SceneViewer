cbuffer cbView : register(b0)
{
    matrix mViewProjection;
    matrix mInvViewProjection;
    matrix mPrevViewProjection;
    float4 CameraOrigin;
    float4 DirectionalLight;
    float4 ViewportSize;
    float Proj_m32;
    float Proj_m22;
    uint FrameNumber;

    float padding0;

    float4 BoundingBoxMin;
    float4 BoundingBoxSize;
    
    float padding[56];
};

struct MeshInfo
{
    matrix mWorld;
    int MeshIdx;
    int AlbedoTextureIdx;
    int NormalTextureIdx;
    int PBRTextureIdx;
    float3 Albedo;
    float Roughness;
    float Metallic;
};

#define PI 3.1415926f

struct VS_INPUT
{
    float4 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Texcoord : TEXCOORD0;
    uint InstanceID : SV_InstanceID;
};

float GGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;

    float a_sqr = a * a;
    float N_dot_H = max(dot(N, H), 0.0f);

    float bottom = N_dot_H * N_dot_H * (a_sqr - 1.0f) + 1.0f;
    return a_sqr / max(bottom * bottom * PI, 0.0001f);
}

float Geometry(float N_dot_V, float N_dot_L, float roughness)
{
    float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
	//float k = roughness*roughness*0.5f;

    float G1 = N_dot_V / (N_dot_V * (1.0f - k) + k);
    float G2 = N_dot_L / (N_dot_L * (1.0f - k) + k);

    return G1 * G2;
}

float3 Fresnel(float3 H, float3 V, float3 F_0)
{
    float H_dot_V = max(dot(H, V), 0.0f);
	//return F_0 + (1.0f -F_0)*pow((1.0f - H_dot_V), 5.0f);
    float F_C = pow((1.0f - H_dot_V), 5.0f);
    return saturate(50.0f * F_0.g) * F_C + (1.0f - F_C) * F_0;
}

float3 CalculatePBR(float3 L, float3 N, float3 V, float roughness, float metal, float3 albedo, float Radiance)
{
    float3 H = normalize(V + L);

    float N_dot_L = max(dot(N, L), 0.0f);
    float N_dot_V = max(dot(N, V), 0.0f);

    float D = GGX(N, H, roughness);
    float G = Geometry(N_dot_V, N_dot_L, roughness);
    float3 SpecularColor = float3(0.04f, 0.04f, 0.04f);
    SpecularColor = lerp(SpecularColor, albedo.rgb, metal);
    float3 F = Fresnel(H, L, SpecularColor);

    float3 Kd = (1.0f - F) * (1.0f - metal);

    float3 Specular = D * G * F / max((4.0f * N_dot_V * N_dot_L), 0.0001f);
    return max((Kd * albedo / PI + Specular) * N_dot_L * Radiance, 0.0f);
}

float3 ACESFitted(float3 color)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0f, 1.0f);
}

float4 GetWorldPositionFromDepth(float Depth, float2 ScreenUV)
{
    float4 ScreenPos;
    ScreenPos.x = ScreenUV.x * 2.0f - 1.0f;
    ScreenPos.y = 1.0f - ScreenUV.y * 2.0f;
    ScreenPos.z = Depth;
    ScreenPos.w = 1.0f;
    
    float4 WldPos = mul(ScreenPos, mInvViewProjection);
    return (WldPos / WldPos.w);
}

struct QuadVS_Output
{
    float4 Pos : SV_POSITION;
    float2 Uv : TEXCOORD0;
};

float4 SHBasisFunc(float3 Dir)
{
    float4 Res;
    Res.x = 0.282095f;
    Res.y = -0.488603f * Dir.y;
    Res.z = 0.488603f * Dir.z;
    Res.w = -0.488603f * Dir.x;
    return Res;
}

float4 SHCosTransfer(float3 Dir)
{
    float4 Res = SHBasisFunc(Dir);
    Res.x *= 3.1415926f;
    Res.yzw *= (3.1415926f * 2.0f / 3.0f);
    return Res;
}

struct SH_RGB
{
    float4 R;
    float4 G;
    float4 B;
};

float3 SampleIrradiance(Texture3D InSHVolumeR, Texture3D InSHVolumeG, Texture3D InSHVolumeB, float3 WldPos, float3 N)
{
    float4 SHTransfer = SHCosTransfer(N);
    
    /*
    float3 Uvw = (WldPos - BoundingBoxMin.xyz) / BoundingBoxSize.xyz;
    float4 SH_R = SHVolumeR.SampleLevel(LinearSampler, Uvw, 0);
    float4 SH_G = SHVolumeG.SampleLevel(LinearSampler, Uvw, 0);
    float4 SH_B = SHVolumeB.SampleLevel(LinearSampler, Uvw, 0);
                    
    float3 ProbeLighting = float3(dot(SH_R, SHTransfer), dot(SH_G, SHTransfer), dot(SH_B, SHTransfer));
    return max(ProbeLighting, 0.0f);
    */
    
    uint3 VolumeResolution;
    InSHVolumeR.GetDimensions(VolumeResolution.x, VolumeResolution.y, VolumeResolution.z);

    float3 VolumeCellSize = (BoundingBoxSize.xyz) / (VolumeResolution - 1);
    
    float3 BiasedWldPos = WldPos + N * 1.5f;
    
    float3 VoxelCoords = (BiasedWldPos - BoundingBoxMin.xyz) / VolumeCellSize;
    int3 BaseIndex = int3(floor(VoxelCoords));
    float3 FracWeights = frac(VoxelCoords);
    
    float3 TotalLighting = float3(0.0f, 0.0f, 0.0f);
    float TotalWeight = 0.0f;
    
    for (int z = 0; z <= 1; ++z)
    {
        for (int y = 0; y <= 1; ++y)
        {
            for (int x = 0; x <= 1; ++x)
            {
                int3 ProbeCoords = BaseIndex + int3(x, y, z);
                
                ProbeCoords = clamp(ProbeCoords, int3(0, 0, 0), int3(VolumeResolution) - 1);
                
                // Compute standard structural trilinear interpolation weight
                float3 TrilinearTerms = float3(x == 1 ? FracWeights.x : 1.0f - FracWeights.x,
                                               y == 1 ? FracWeights.y : 1.0f - FracWeights.y,
                                               z == 1 ? FracWeights.z : 1.0f - FracWeights.z);
                float CombinedWeight = TrilinearTerms.x * TrilinearTerms.y * TrilinearTerms.z;
                
                /*
                float3 ProbeWldPos = BoundingBoxMin.xyz + (float3(ProbeCoords)) * VolumeCellSize;
                float3 DirToPixel = BiasedWldPos - ProbeWldPos;
                float W = max(dot(N, normalize(DirToPixel)), 0.001f);
                CombinedWeight *= W;
                */
                
                //if (CombinedWeight > 0.001f)
                {
                    float4 SH_R = InSHVolumeR.Load(int4(ProbeCoords, 0));
                    float4 SH_G = InSHVolumeG.Load(int4(ProbeCoords, 0));
                    float4 SH_B = InSHVolumeB.Load(int4(ProbeCoords, 0));
                    
                    float3 ProbeLighting = float3(dot(SH_R, SHTransfer), dot(SH_G, SHTransfer), dot(SH_B, SHTransfer));
                    
                    TotalLighting += max(0.0f, ProbeLighting) * CombinedWeight;
                    TotalWeight += CombinedWeight;
                }
            }
        }
    }
    
    return TotalLighting / max(0.0001f, TotalWeight);
}
