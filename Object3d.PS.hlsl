#include "Object3d.hlsli"

cbuffer Material : register(b0)
{
    float4 gMaterialColor;
    int enableLighting;
    float3 _padding0;
    float4x4 gMaterialUVTransform;
    float shininess;
};

cbuffer DirectionalLight : register(b1)
{
    float4 gDirectionalLightColor;
    float3 gDirectionalLightDirection;
    float gDirectionalLightIntensity;
};

cbuffer Camera : register(b2)
{
    float3 gCameraWorldPosition;
    float _padding1;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV 変換
    float4 uv4 = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterialUVTransform);
    float2 uv = uv4.xy;

    // テクスチャ
    float4 textureColor = gTexture.Sample(gSampler, uv);

    // 法線・ライト・視線
    float3 N = normalize(input.normal);
    float3 L = normalize(-gDirectionalLightDirection); // ライト方向の扱いに合わせる
    float3 V = normalize(gCameraWorldPosition - input.worldPosition);

    // 拡散反射
    float NdotL = saturate(dot(N, L));
    float3 diffuse = gMaterialColor.rgb * textureColor.rgb * gDirectionalLightColor.rgb * NdotL * gDirectionalLightIntensity;

    // 鏡面反射（必要なら）
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (enableLighting != 0)
    {
        float3 R = reflect(-L, N);
        float RdotV = saturate(dot(R, V));
        float specPow = pow(RdotV, max(shininess, 1e-6f));
        specular = gDirectionalLightColor.rgb * gDirectionalLightIntensity * specPow;
    }

    output.color.rgb = diffuse + specular;
    output.color.a = gMaterialColor.a * textureColor.a;

    // アルファが完全に0なら破棄（必要最小限）
    if (output.color.a <= 0.0f)
    {
        discard;
    }

    return output;
}