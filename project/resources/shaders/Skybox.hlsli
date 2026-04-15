// 頂点シェーダーからピクセルシェーダーへ渡すデータの構造体
struct VertexShaderOutput
{
    float4 pos : SV_POSITION; // システム用頂点座標
    float3 texcoord : TEXCOORD; // キューブマップサンプリング用の3D方向ベクトル
};

// ビュー・プロジェクション行列（C++側から送られてくる定数バッファ）
cbuffer ViewProjection : register(b0)
{
    matrix view; // ビュー行列
    matrix projection; // プロジェクション行列
};