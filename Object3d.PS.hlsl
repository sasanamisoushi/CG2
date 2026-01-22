#include "object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransfoem;
    float32_t shiniess;
};

struct Camera
{
    float32_t3 worldPosition;
};

ConstantBuffer<Camera> gCamera : register(b2);
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



struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float32_t4(input.texcoord,0.0f, 1.0f), gMaterial.uvTransfoem);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    //入射光の反射ベクトルを求める
    float32_t3 reflectLight = reflect(gDirectionalLight.direction, normalize(input.normal));
 
    float RdotE = dot(reflectLight, toEye);
    float speularPow = pow(saturate(RdotE), gMaterial.shiniess);//反射強度
    
    //拡散反射
    float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
    //鏡面反射
    float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * speularPow * float32_t3(1.0f, 1.0f, 1.0f);
    //拡散反射＋鏡面反射
    output.color.rgb = diffuse + specular;
    //アルファは今まで通り
    output.color.a = gMaterial.color.a * textureColor.a;
    
    
    if (gMaterial.enableLighting != 0)
    {
         float Ndotl = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(Ndotl * 0.5f + 0.5f, 2.0f);
        output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
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




