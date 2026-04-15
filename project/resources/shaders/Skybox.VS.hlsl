#include "Skybox.hlsli"

// C++側の頂点バッファから受け取る入力データ
// ※スカイボックスは法線や2DのUVが不要なので、位置(xyz)だけでOKです
struct VertexShaderInput
{
    float3 pos : POSITION;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // カメラの平行移動成分を無視する（回転のみ適用）
    matrix viewNoTranslation = view;
    viewNoTranslation[3][0] = 0.0f;
    viewNoTranslation[3][1] = 0.0f;
    viewNoTranslation[3][2] = 0.0f;

    // 行列の合成と座標変換
    matrix viewProj = mul(viewNoTranslation, projection);
    output.pos = mul(float4(input.pos, 1.0f), viewProj);

    // Z = W にすることで、深度テスト後に常に最奥(Z=1.0)になるようにする
    output.pos.z = output.pos.w;

    // 頂点のローカル座標をそのままテクスチャサンプリング用の3Dベクトルとして渡す
    output.texcoord = input.pos;

    return output;
}