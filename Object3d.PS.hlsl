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
    
    float4 transformedUV = mul(float32_t4(input.texcoord,0.0f, 1.0f), gMaterial.uvTransfoem);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
 
    if (gMaterial.enableLighting != 0)
    {
        
       
        float cos = 1.0f;
        float lightingFactor = 1.0f;
        if (lightingMode == 1.0f)
        {
           cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));

           
        }
        else if(lightingMode == 2.0f)
        {
            float Ndotl = dot(normalize(input.normal), -gDirectionalLight.direction);
            cos = pow(Ndotl * 0.5f + 0.5f, 2.0f);
        }
        
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    return output;
}




