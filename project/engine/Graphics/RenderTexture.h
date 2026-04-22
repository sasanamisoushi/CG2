#pragma once
#include "engine/Graphics/DirectXCommon.h"
#include <wrl.h>
#include <d3d12.h>
#include <string>


class RenderTexture {
public:
    // 初期化：テクスチャの幅と高さを指定
    void Initialize(uint32_t width, uint32_t height);

    // ゲッター
    ID3D12Resource *GetResource() const { return resource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle() const { return rtvHandle_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const { return srvHandleGPU_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;

    // 描画先として使うためのハンドル (RTV)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_;

    // 画像として使うためのハンドル (SRV)
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_;
};

