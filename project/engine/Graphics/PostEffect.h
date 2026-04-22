#pragma once
#include "engine/Graphics/DirectXCommon.h"
#include <wrl.h>

class PostEffect {
public:
    void Initialize();
    // 描画には、RenderTextureの画像(SRV)のGPUハンドルを渡す
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE srvHandle);

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};