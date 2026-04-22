#include "CopyImage.hlsli"


Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// C++から送られてくる「定数」を受け取る箱
cbuffer PostEffectParam : register(b0)
{
    int effectType; // エフェクトの種類
    float vignetteRadius; // ヴィネットの半径
    float vignetteSoftness; // ヴィネットのぼかし具合
    float blurIntensity; // ぼかしの強さ
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
    else if (effectType == 4)
    {
        // 4: ヴィネッティング
        // UV座標の中心は (0.5, 0.5)
        float2 center = float2(0.5f, 0.5f);
        
        // 中心からの距離を計算（中心なら0.0、四隅なら最大約0.7）
        float dist = distance(input.texcoord, center);
        
        // smoothstep(min, max, x) で滑らかなグラデーションを作る
        // 例: 距離0.3までは明るいまま、0.8に向けて徐々に暗くする
        float vignette = 1.0f - smoothstep(vignetteRadius, vignetteRadius + vignetteSoftness, dist);
        
        // 元の色にヴィネット係数（0.0～1.0）を掛け算して暗くする
        output.color = float4(texColor.rgb * vignette, texColor.a);
    }
    else if (effectType == 5)
    {
        // 5: ボックスフィルタ (3x3ピクセルの平均ブラー)
        
        // 1. テクスチャの解像度を取得し、1ピクセルあたりのUVサイズを計算する
        uint width, height;
        gTexture.GetDimensions(width, height);
        float2 uvStep = float2(1.0f / width, 1.0f / height);

        // ぼかしの強さ（2.0fや3.0fにすると、より遠くのピクセルを拾って強くぼけます）
        float intensity = 1.5f;

        // 色を足し合わせるための箱を用意
        float4 colorSum = 0;
        
        // 2. 中心を含む周囲9ピクセル（タテ3 × ヨコ3）の色をサンプリングして足す
        for (int y = -1; y <= 1; ++y)
        {
            for (int x = -1; x <= 1; ++x)
            {
                // 現在のUV座標から少しだけズラした位置を計算
                float2 offset = float2(x, y) * uvStep * blurIntensity;
                colorSum += gTexture.Sample(gSampler, input.texcoord + offset);
            }
        }
        
        // 3. 9回足したので、最後に9で割って「平均色」にする
        output.color = colorSum / 9.0f;
    }
    else if (effectType == 6)
    {
        // 6: 5x5 ボックスフィルタ
        uint width, height;
        gTexture.GetDimensions(width, height);
        float2 uvStep = float2(1.0f / width, 1.0f / height);
        
        float4 colorSum = float4(0.0f, 0.0f, 0.0f, 0.0f);
        
        // ★変更点1：ループの範囲を -2 から 2 に広げる
        for (int y = -2; y <= 2; ++y)
        {
            for (int x = -2; x <= 2; ++x)
            {
                float2 offset = float2(x, y) * uvStep * blurIntensity;
                colorSum += gTexture.Sample(gSampler, input.texcoord + offset);
            }
        }
        
        // ★変更点2：5x5 = 25回足したので、25.0fで割る
        output.color = colorSum / 25.0f;
    }
    else if (effectType == 7)
    {
        // 7: ガウシアンブラー (5x5)
        uint width, height;
        gTexture.GetDimensions(width, height);
        float2 uvStep = float2(1.0f / width, 1.0f / height);

        // ガウシアンカーネル（5x5 = 25個の重み）
        // 中心が一番大きく(0.1406)、端に行くほど小さくなる(0.0039)
        // 合計するとピッタリ 1.0 になります
        float kernel[25] =
        {
            0.0039f, 0.0156f, 0.0234f, 0.0156f, 0.0039f,
            0.0156f, 0.0625f, 0.0937f, 0.0625f, 0.0156f,
            0.0234f, 0.0937f, 0.1406f, 0.0937f, 0.0234f,
            0.0156f, 0.0625f, 0.0937f, 0.0625f, 0.0156f,
            0.0039f, 0.0156f, 0.0234f, 0.0156f, 0.0039f
        };

        float4 colorSum = float4(0.0f, 0.0f, 0.0f, 0.0f);
        
        // ループで周囲のピクセルを拾い、対応する「重み」を掛けて足し合わせる
        for (int y = -2; y <= 2; ++y)
        {
            for (int x = -2; x <= 2; ++x)
            {
                float2 offset = float2(x, y) * uvStep * blurIntensity;
                
                // 配列のインデックス（0～24）を計算
                int index = (y + 2) * 5 + (x + 2);
                
                // ピクセルの色に「重み（kernel）」を掛け算する
                colorSum += gTexture.Sample(gSampler, input.texcoord + offset) * kernel[index];
            }
        }
        
        // 重みの合計はすでに1.0になっているので、割り算は不要！
        output.color = colorSum;
    }
    else
    {
        // 0: ノーマル（そのまま）
        output.color = texColor;
    }
    return output;
}