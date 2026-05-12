// Skinning.CS.hlsl
#include "Skinning.hlsli"

// 行列パレット
StructuredBuffer<WellKnownPalette> gMatrixPalette : register(t0);
// 入力頂点 (VertexBufferの代わり)
StructuredBuffer<Vertex> gInputVertices : register(t1);
// スキニング情報の定数バッファ
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

// スキニング後の頂点データを出力する (UAV)
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);

[numthreads(64, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t vertexIndex = DTid.x;
    if (vertexIndex >= gSkinningInformation.numVertices) {
        return;
    }

    // 入力データの取得
    Vertex input = gInputVertices[vertexIndex];

    // スキニング計算
    float32_t4 position = float32_t4(0.0, 0.0, 0.0, 0.0);
    float32_t3 normal = float32_t3(0.0, 0.0, 0.0);
 
    for (int32_t i = 0; i < 4; ++i) {
        float32_t weight = input.weight[i];
        if (weight <= 0.0f) continue;
        
        int32_t boneIndex = input.index[i];
        if (boneIndex < 0 || (uint32_t)boneIndex >= gSkinningInformation.numBones) continue;
        
        // 行列による変換 (wを1.0にすることで平行移動を適用する)
        position += mul(float32_t4(input.position.xyz, 1.0f), gMatrixPalette[boneIndex].skeletonSpaceMatrix) * weight;
        // 法線は回転・拡縮成分(3x3)で変換
        normal += mul(input.normal.xyz, (float32_t3x3)gMatrixPalette[boneIndex].skeletonSpaceMatrix) * weight;
    }

    // 計算結果の書き込み
    position.w = 1.0f;
    gOutputVertices[vertexIndex].position = position;
    // 法線の長さが0でないことを確認して正規化
    float32_t normalLength = length(normal);
    if (normalLength > 0.0001f) {
        gOutputVertices[vertexIndex].normal = float32_t4(normal / normalLength, 0.0f);
    } else {
        gOutputVertices[vertexIndex].normal = float32_t4(0.0f, 1.0f, 0.0f, 0.0f); // デフォルト法線
    }
    gOutputVertices[vertexIndex].texcoord = input.texcoord;
    gOutputVertices[vertexIndex].color = input.color;
}
