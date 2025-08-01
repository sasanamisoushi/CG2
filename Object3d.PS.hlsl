#include "object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransfoem;
};



ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};


ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

cbuffer Scene : register(b2)
{
    float3 cameraPos;
    float lightingMode; // 0: None, 1: CosFalloff, 2: Lambert
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransfoem);
    float4 baseColor = gTexture.Sample(gSampler, transformedUV.xy);
    float3 finalColor = (gMaterial.color * baseColor).rgb;

    if (gMaterial.enableLighting != 0 && lightingMode > 0.5f)
    {
        float3 normal = normalize(input.normal);
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float lightIntensity = 1.0f;

        if (lightingMode == 1.0f)
        {
            lightIntensity = saturate(dot(normal, lightDir));
        }
        else if (lightingMode == 2.0f)
        {
            float NdotL = dot(normal, lightDir);
            lightIntensity = pow(NdotL * 0.5f + 0.5f, 2.0f);
        }

        finalColor *= gDirectionalLight.color.rgb * gDirectionalLight.intensity * lightIntensity;
    }

    output.color = float4(finalColor, baseColor.a);
    return output;
}




