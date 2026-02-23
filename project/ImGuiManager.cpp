#include "ImGuiManager.h"
#include <cstdint>
#include "SrvManager.h"

void ImGuiManager::Initialize(HWND hwnd, ID3D12Device *device, int numFramesInFlight, DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap *srvHeap) {
#ifdef ENABLE_IMGUI
	// ImGuiのコンテキストを作成
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO(); (void)io;

	// スタイル設定
	ImGui::StyleColorsDark();

	// WIn32とDX12のバックエンドを初期化
	ImGui_ImplWin32_Init(hwnd);
	// SrvManagerから、ImGuiフォント用の空きインデックスを1つもらう
	uint32_t srvIndex = SrvManager::GetInstance()->Allocate();

	// そのインデックスのハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);

	ImGui_ImplDX12_Init(device, numFramesInFlight, rtvFormat, srvHeap, cpuHandle, gpuHandle);
#endif
}

void ImGuiManager::Shutdown() {
#ifdef ENABLE_IMGUI
	// 逆順で終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}

void ImGuiManager::BeginFrame() {
#ifdef ENABLE_IMGUI
	// ImGuiの新しいフレームを開始
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void ImGuiManager::EndFrame(ID3D12GraphicsCommandList *commandList) {
#ifdef ENABLE_IMGUI
	//描画データを生成
	ImGui::Render();

	//デスクリプタヒープをセット
	//commandList->SetDescriptorHeaps(1, &srvHeap);

	//コマンドリストにImGuiの描画コマンドを積む
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}


