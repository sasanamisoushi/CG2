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
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 color : COLOR0;
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

struct WellKnownPalette
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceNormalMatrix;
};
StructuredBuffer<WellKnownPalette> gMatrixPalette : register(t0);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // スキニング計算
    float32_t4 skinnedPosition = input.position;
    float32_t3 skinnedNormal = input.normal;
    
    // sumOfWeightが0より大きければスキニングモデルと判定
    float sumOfWeight = input.weight.x + input.weight.y + input.weight.z + input.weight.w;
    if (sumOfWeight > 0.0f) {
        float32_t4x4 skinnedMatrix = 
            gMatrixPalette[input.index.x].skeletonSpaceMatrix * input.weight.x +
            gMatrixPalette[input.index.y].skeletonSpaceMatrix * input.weight.y +
            gMatrixPalette[input.index.z].skeletonSpaceMatrix * input.weight.z +
            gMatrixPalette[input.index.w].skeletonSpaceMatrix * input.weight.w;
            
        float32_t4x4 skinnedNormalMatrix = 
            gMatrixPalette[input.index.x].skeletonSpaceNormalMatrix * input.weight.x +
            gMatrixPalette[input.index.y].skeletonSpaceNormalMatrix * input.weight.y +
            gMatrixPalette[input.index.z].skeletonSpaceNormalMatrix * input.weight.z +
            gMatrixPalette[input.index.w].skeletonSpaceNormalMatrix * input.weight.w;
            
        skinnedPosition = mul(input.position, skinnedMatrix);
        skinnedNormal = mul(input.normal, (float32_t3x3)skinnedNormalMatrix);
    }
    
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    output.texcoord = transformedUV.xy;
    output.normal = normalize(mul(skinnedNormal, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    output.color = input.color;
    return output;
}