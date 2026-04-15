#pragma once
#include "engine/Graphics/DirectXCommon.h"
#include <wrl.h>
#include <memory>

class SkyboxCommon {
public:
    static SkyboxCommon *GetInstance();

    void Initialize(DirectXCommon *dxCommon);

    // 描画前の共通設定（RootSignatureとPSOのセット）
    void SetCommonDrawSettings();

    // ゲッター
    DirectXCommon *GetDxCommon() const { return dxCommon_; }
    ID3D12RootSignature *GetRootSignature() const { return rootSignature_.Get(); }

private:
    SkyboxCommon() = default;
    ~SkyboxCommon() = default;
    SkyboxCommon(const SkyboxCommon &) = delete;
    SkyboxCommon &operator=(const SkyboxCommon &) = delete;

    void CreateRootSignature();
    void CreateGraphicsPipeline();

    DirectXCommon *dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};

