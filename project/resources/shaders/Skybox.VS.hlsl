#include "Skybox.hlsli"

// C++蛛ｴ縺ｮ鬆らせ繝舌ャ繝輔ぃ縺九ｉ蜿励￠蜿悶ｋ蜈･蜉帙ョ繝ｼ繧ｿ
// ※スカイボックスは法線や2DのUVが不要なので、位置(xyz)だけでOKです。
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

    // 陦悟・縺ｮ蜷域・縺ｨ蠎ｧ讓吝､画鋤
    matrix viewProj = mul(viewNoTranslation, projection);
    output.pos = mul(float4(input.pos, 1.0f), viewProj);

    // Z = W 縺ｫ縺吶ｋ縺薙→縺ｧ縲∵ｷｱ蠎ｦ繝・せ繝亥ｾ後↓蟶ｸ縺ｫ譛螂･(Z=1.0)縺ｫ縺ｪ繧九ｈ縺・↓縺吶ｋ
    output.pos.z = output.pos.w * 0.99999f;

    // 頂点のローカル座標をそのままテクスチャサンプリング用の3Dベクトルとして渡す
    output.texcoord = input.pos;

    return output;
}
