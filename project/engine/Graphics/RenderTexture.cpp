#include "RenderTexture.h"
#include "engine/Graphics/SrvManager.h"
#include <cassert>

void RenderTexture::Initialize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;

    DirectXCommon *dxCommon = DirectXCommon::GetInstance();
    auto device = dxCommon->GetDevice();

    D3D12_RESOURCE_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.DepthOrArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.SampleDesc.Count = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = 0.0f; // R
    clearValue.Color[1] = 0.0f; // G
    clearValue.Color[2] = 0.0f; // B
    clearValue.Color[3] = 1.0f; // A

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&resource_)
    );
    assert(SUCCEEDED(hr));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    if (rtvHandle_.ptr == 0) {
        rtvHandle_ = dxCommon->GetNewRtvHandle();
    }
    device->CreateRenderTargetView(resource_.Get(), nullptr, rtvHandle_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (srvHandleCPU_.ptr == 0) {
        uint32_t srvIndex = SrvManager::GetInstance()->Allocate();
        srvHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex);
        srvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);
    }
    device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);

    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void RenderTexture::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    if (width == width_ && height == height_) {
        return;
    }

    resource_.Reset();
    Initialize(width, height);
}

void RenderTexture::Transition(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
}
