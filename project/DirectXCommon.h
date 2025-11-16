#pragma once
#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "WinApp.h"

class DirectXCommon {

public:
	//初期化
	void Initialize(WinApp *winApp);

	//デバイスの初期化
	void CreateDevice();

	//コマンド関連の初期化
	void CreateCommand();

	//スワップチェーンの初期化
	void CreateSwapChain();

	//深度バッファの生成
	void depthBufferGeneration(int32_t width, int32_t height);

	//各種デスクリプタヒープの生成
	void CreateDescriptorHeaps();

	//デスクリプタヒープを生成する
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
		CreateDescriptorHesp(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	//レンダーターゲットビューの初期化
	void CreateRenderTargetView();

	//SRVの指定番号のCPUデスクリプトハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

	//SRVの指定番号のGPUデスクリプトハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

	//深度ステンシルビューの初期化
	void CreateDepthStencilView();

	//フェンスの初期化
	void CreateFence();

	//ビューポート矩形の初期化
	void CreateViewportScissorRect();

	//シザリング矩形の初期化
	void CreateScissorRect();

	//DXCコンパイラの生成
	void CreateDXCCompiler();

	//ImGuiの初期化
	void InitializeImGui();

	//描画前処理
	void PreDraw();

	//描画後処理
	void PostDraw();



private:

	//DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device;

	//DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;

	//コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;

	//コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;

	//コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;

	//WindownAPI
	WinApp *winApp = nullptr;

	//RTV用のデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = nullptr;

	// SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = nullptr;

	// DSV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = nullptr;

	//深度ステンシルバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;

	// デスクリプタサイズ（SRV用）
	uint32_t descriptorSizeSRV = 0;
	// デスクリプタサイズ（RTV用）
	uint32_t descriptorSizeRTV = 0;
	// デスクリプタサイズ（DSV用）
	uint32_t descriptorSizeDSV = 0;

	//スワップチェーンリソース
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources;

	//スワップチェーン設定
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	//RTVフォーマット
	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	//RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	//ビューポート
	D3D12_VIEWPORT viewport{};

	//シザー矩形
	D3D12_RECT scissorRecct{};

	//DXCコンパイラ
	Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;

	//フェンス用イベントハンドル
	HANDLE fenceEvent = nullptr;

	//フェンス値
	uint64_t fenceValue = 0;

	//TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier{};


	//指定番号のCPUデスクリプトハンドルを取得
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//指定番号のGPUデスクリプトハンドルを取得
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> &descriptorHeap, uint32_t descriptorSize, uint32_t index);



};


