#pragma once
#include "engine/Graphics/DirectXCommon.h"
#include <wrl.h>

class PostEffect {
public:
    // パラメータをまとめた構造体（HLSLと並びを合わせる）
    struct PostEffectParam {
        int effectType;
        float vignetteRadius;
        float vignetteSoftness;
        float blurIntensity;
    };

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

    // パラメータの実体と初期値
    PostEffectParam param_ = { 0, 0.3f, 0.5f, 1.5f };
};