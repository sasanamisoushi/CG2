#pragma once
#include "engine/Graphics/DirectXCommon.h"
#include <wrl.h>

class PostEffect {
public:
    void Initialize();
    // 描画には、RenderTextureの画像(SRV)のGPUハンドルを渡す
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE srvHandle);

    // ImGuiを描画する関数を追加
    void DrawImGui();

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // 現在のエフェクトの種類を保持する変数 (0:通常, 1:モノクロ, 2:反転)
    int effectType_ = 0;
};