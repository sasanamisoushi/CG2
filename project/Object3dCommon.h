#pragma once
#include <DirectXCommon.h>
class Object3dCommon {
public:
	//初期化
	void Initialize(DirectXCommon* dxCommon);

	//共通描画設定
	void SetCommonDrawSettings();

	//シェーダーの読み込み
	void LoadShaders();

	DirectXCommon *GetDxCommon()const { return dxCommon_; }

private:
	//ルートシグネチャの作成
	void CreateRootSignature();

	//グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();


	DirectXCommon *dxCommon_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	// Shader blobs
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
};

