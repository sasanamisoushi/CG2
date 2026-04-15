#include "Skybox.hlsli"

// Texture2Dではなく、TextureCubeを使う
TextureCube<float4> skyboxTexture : register(t0);
SamplerState smp : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // 3Dベクトル(texcoord)を使ってキューブマップから色を取得
    return skyboxTexture.Sample(smp, input.texcoord);
}