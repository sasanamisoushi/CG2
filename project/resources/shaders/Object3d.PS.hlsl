#include "object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
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

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

    // 1. 光源へ向かうベクトルを求める (directionは光の進む向きなのでマイナスをかける)
    float32_t3 lightDirection = normalize(-gDirectionalLight.direction);

    // 2. 視線(toEye)と光(lightDirection)のハーフベクトルを求める
    float32_t3 halfVector = normalize(lightDirection + toEye);

    // 3. 法線とハーフベクトルの内積を求める
    float NDotH = dot(normalize(input.normal), halfVector);

    // 4. ハイライトの強さを計算
    float specularPow = pow(saturate(NDotH), gMaterial.shininess);
    
    
    if (gMaterial.enableLighting != 0)
    {
         float Ndotl = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(Ndotl * 0.5f + 0.5f, 2.0f);
        
        float3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        
        float3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float3(1.0f, 1.0f, 1.0f);
        
        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    //textureのa値が0.5以下のときにPixelを破棄
    if (textureColor.a <= 0.5)
    {
        discard;
    }
    
     //textureのa値が0のときにPixelを破棄
    if (textureColor.a == 0.0)
    {
        discard;
    }
    
     //output.colorのa値が0.0以下のときにPixelを破棄
    if (output.color.a <= 0.0)
    {
        discard;
    }
    return output;
}




