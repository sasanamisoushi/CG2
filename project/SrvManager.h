#pragma once
#include "DirectXCommon.h"

//SRV管理
class SrvManager {
public:

	// シングルトンインスタンスの取得
	static SrvManager *GetInstance();

	// 終了処理
	void Finalize();

	//初期化
	void Initialize(DirectXCommon *dxCommon);

	// デスクリプタヒープの取得（★追加：これをDirectXCommon::PreDrawで使う）
	ID3D12DescriptorHeap *GetDescriptorHeap() const { return descriptorHeap.Get(); }

	uint32_t Allocate();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	//SRV生成
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource *pResource, DXGI_FORMAT Format, UINT MipLevels);

	//SRV生成
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource *pResource, UINT numElements, UINT structureByteStride);

	void PreDraw();

	//SRVセットコマンド
	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);


private:

	static SrvManager *instance;

	SrvManager() = default;
	~SrvManager() = default;
	SrvManager(const SrvManager &) = delete;
	SrvManager &operator=(const SrvManager &) = delete;

	DirectXCommon *directXCommon = nullptr;

	//最大SRV数
	static const uint32_t kMaxSRVCount;

	//SRV用のデスクリプタサイズ
	uint32_t descriptorSize;
	//SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	//次に使用するSRVインデックス
	uint32_t useIndex = 0;

};

