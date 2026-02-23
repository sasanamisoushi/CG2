#pragma once
#include<Windows.h>
#include <d3d12.h>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

class ImGuiManager {
public:
	//初期化
	//　hwnd:ウインドハンドル
	// device:DirectX12のデバイス
	// numFramesInFlight:スワップチェーンのバッファ数
	// rtvFormat:レンダーターゲットのフォーマット
	// srvHeap: ImGuiのフォントテクスチャ等を配置するSRVデスクリプタヒープ
	void Initialize(HWND hwnd, ID3D12Device *device, int numFramesInFlight, DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap *srvHeap);

	void Shutdown();

	//毎フレームの処理
	void BeginFrame();

	// CommandList: ImGuiの描画コマンドを起動するコマンドリスト
	void EndFrame(ID3D12GraphicsCommandList *commandList);
};

