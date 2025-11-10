#pragma once
#include "Logger.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

class DirectXCommon {

public:
	//初期化
	void Initialize();

	//デバイスの初期化
	void CreateDevice();

	//DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device8> device;

	//DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
};

