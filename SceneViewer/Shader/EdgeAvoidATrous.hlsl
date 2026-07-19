// https://www.highperformancegraphics.org/previous/www_2010/media/RayTracing_I/HPG2010_RayTracing_I_Dammertz.pdf

cbuffer AtrousBuffer : register(b0)
{
    int g_StepSize; // 1, 2, 4, 8, etc.
    float g_PhiColor; // 50
    float g_PhiNormal; // 128.0
    float g_PhiDepth; // 0.01 to 0.05
    float Proj_m32;
    float Proj_m22;
    uint ViewportW;
    uint ViewportH;
};

Texture2D<float4> ColorTexture : register(t0); // RGB = Indirect Diffuse
Texture2D<float4> NormalTexture : register(t1);
Texture2D<float> DepthBuffer : register(t2); // Linear depth

RWTexture2D<float4> OutputColor : register(u0); // Output filtered color and variance

// B3-Spline 5x5 weights
static const float g_KernelWeights[5] = { 1.0f / 16.0f, 1.0f / 4.0f, 3.0f / 8.0f, 1.0f / 4.0f, 1.0f / 16.0f };

float GetLuminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

float GetLinearZ(float InDepth)
{
    return Proj_m32 / (InDepth - Proj_m22);
}

[numthreads(16, 16, 1)]
void main(uint3 dtID : SV_DispatchThreadID)
{
    int2 centerCoord = dtID.xy;
    
    // Read center pixel parameters
    float3 centerColor = ColorTexture[centerCoord].rgb;
    
    float3 centerNormal = NormalTexture[centerCoord].rgb;
    float centerDepth = DepthBuffer[centerCoord];
    
    float3 centerN = normalize(centerNormal.xyz * 2.0f - 1.0f);
    
    if (centerDepth >= 1.0f || centerDepth <= 0.0f || length(centerN) < 0.01f)
    {
        OutputColor[centerCoord] = float4(centerColor, 1.0f);
        return;
    }
    
    centerDepth = GetLinearZ(centerDepth);
    
    float3 totalColor = 0.0f;
    float totalWeight = 0.0f;

    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            int2 offset = int2(x, y) * g_StepSize;
            int2 sampleCoord = centerCoord + offset;
            
            float3 sampleColor = ColorTexture[sampleCoord].rgb;
            
            float3 sampleNormal = NormalTexture[sampleCoord].rgb;
            float3 sampleN = normalize(sampleNormal * 2.0f - 1.0f);
            float sampleDepth = GetLinearZ(DepthBuffer[sampleCoord]);

            float depthDiff = abs(centerDepth - sampleDepth);
            float wDepth = exp(-depthDiff / (g_PhiDepth * centerDepth + 1e-4f));

            float normalDiff = max(0.0f, dot(centerN, sampleN));
            float wNormal = pow(normalDiff, g_PhiNormal);

            float3 colorDiff = abs(centerColor - sampleColor);
            float wColor = exp(-dot(colorDiff, colorDiff) / g_PhiColor);

            // Combine edge weights with structural B3-Spline weight
            float kernelWeight = g_KernelWeights[x + 2] * g_KernelWeights[y + 2];
            float isInBounds = (sampleCoord.x >= 0 && sampleCoord.x < ViewportW &&
                    sampleCoord.y >= 0 && sampleCoord.y < ViewportH) ? 1.0f : 0.0f;
            float weight = wColor * wDepth * wNormal * kernelWeight * isInBounds;
            
            totalColor += sampleColor * weight;
            totalWeight += weight;
        }
    }

    // Normalize results
    if (totalWeight > 0.0001f)
    {
        totalColor /= totalWeight;
    }
    else
    {
        totalColor = centerColor;
    }

    OutputColor[centerCoord] = float4(totalColor, 1.0f);
}