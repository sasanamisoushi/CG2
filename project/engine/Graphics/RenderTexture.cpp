#include "RenderTexture.h"
#include "engine/Graphics/SrvManager.h"
#include <cassert>

void RenderTexture::Initialize(uint32_t width, uint32_t height) {

    // ★追加：DirectXCommonのインスタンスを取得して、変数「dxCommon」に入れる
    DirectXCommon *dxCommon = DirectXCommon::GetInstance();

    auto device = dxCommon->GetDevice();

    // 1. リソース設定
    D3D12_RESOURCE_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.DepthOrArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.SampleDesc.Count = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    // ★重要：描画先として許可するフラグ
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // 2. 利用するヒープの設定（GPU専用の高速な場所に作る）
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    // 3. クリア値の設定（背景色：宇宙に合わせて黒など）
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = 0.0f; // R
    clearValue.Color[1] = 0.0f; // G
    clearValue.Color[2] = 0.0f; // B
    clearValue.Color[3] = 1.0f; // A

    // 4. リソース生成
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, // 最初は描画待ち状態
        &clearValue,
        IID_PPV_ARGS(&resource_)
    );
    assert(SUCCEEDED(hr));

    // スライド1枚目：RTVの作成

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    // 5. RTV（描画先の窓口）の作成
    // ※RTV用のディスクリプタヒープから場所を確保する処理が必要です
    // 今回は簡易的に「専用のRTVヒープがある」前提の擬似コードです
     rtvHandle_ = dxCommon->GetNewRtvHandle(); 
     device->CreateRenderTargetView(resource_.Get(), nullptr, rtvHandle_);


     // スライド2枚目：SRVの作成
     D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
     srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
     srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
     srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
     srvDesc.Texture2D.MipLevels = 1;

    // 6. SRV（画像として使う窓口）の作成
    // あなたのエンジンの SrvManager を利用します
    uint32_t srvIndex = SrvManager::GetInstance()->Allocate();
    srvHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex);
    srvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);

    

    device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);
}
