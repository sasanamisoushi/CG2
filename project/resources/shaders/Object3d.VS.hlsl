#include "object3d.hlsli"
 
struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);
 
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};
ConstantBuffer<Material> gMaterial : register(b0);
 
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t4 texcoord : TEXCOORD0;
    float32_t4 normal : NORMAL0;
    float32_t4 color : COLOR0;
};
 
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // スキニングはComputeShaderで実行済みのため、ここでは通常の行列変換のみ行う
    output.position = mul(input.position, gTransformationMatrix.WVP);
    float32_t4 transformedUV = mul(float32_t4(input.texcoord.xy, 0.0f, 1.0f), gMaterial.uvTransform);
    output.texcoord = transformedUV.xy;
    output.normal = normalize(mul(input.normal.xyz, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    output.color = input.color;
    return output;
}