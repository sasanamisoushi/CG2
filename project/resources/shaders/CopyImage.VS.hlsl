#include "CopyImage.hlsli"

// 頂点バッファ(C++)は不要！IDだけで全画面を覆う三角形を錬成する
VertexShaderOutput main(uint id : SV_VertexID)
{
    VertexShaderOutput output;
    // 0, 1, 2のIDから、UV座標(0,0)(2,0)(0,2)を作り出す
    output.texcoord = float2((id << 1) & 2, id & 2);
    // UV座標から画面の座標(-1 ～ 1)に変換する
    output.position = float4(output.texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}