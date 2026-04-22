#include "CopyImage.hlsli"


Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// C++から送られてくる「定数」を受け取る箱
cbuffer PostEffectParam : register(b0)
{
    int effectType; // 0:Normal, 1:Grayscale, 2:Invert
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 texColor = gTexture.Sample(gSampler, input.texcoord);
    
    // effectType の値によって色を切り替える
    if (effectType == 1)
    {
        // 1: モノクロ
        float gray = texColor.r * 0.299f + texColor.g * 0.587f + texColor.b * 0.114f;
        output.color = float4(gray, gray, gray, texColor.a);
    }
    else if (effectType == 2)
    {
        // 2: 色反転
        output.color = float4(1.0f - texColor.r, 1.0f - texColor.g, 1.0f - texColor.b, texColor.a);
    }
    else if (effectType == 3)
    {
        // 3: セピア (NTSC規格などでよく使われるセピア化の係数)
        float r = (texColor.r * 0.393f) + (texColor.g * 0.769f) + (texColor.b * 0.189f);
        float g = (texColor.r * 0.349f) + (texColor.g * 0.686f) + (texColor.b * 0.168f);
        float b = (texColor.r * 0.272f) + (texColor.g * 0.534f) + (texColor.b * 0.131f);
        output.color = float4(r, g, b, texColor.a);
    }
    else
    {
        // 0: ノーマル（そのまま）
        output.color = texColor;
    }
    return output;
}