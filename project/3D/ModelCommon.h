#pragma once
#include "engine/Graphics/DirectXCommon.h"

class ModelCommon {
public:
	//初期化
	void Initialize(DirectXCommon *dxCommon);

	DirectXCommon *GetDxCommon()const { return dxCommon_; }

	ID3D12RootSignature* GetSkinningRootSignature() const { return skinningRootSignature_.Get(); }
	ID3D12PipelineState* GetSkinningPipelineState() const { return skinningPipelineState_.Get(); }

private:
	DirectXCommon *dxCommon_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningPipelineState_;
};

