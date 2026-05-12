#include "ModelCommon.h"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <cassert>

void ModelCommon::Initialize(DirectXCommon *dxCommon) {
	dxCommon_ = dxCommon;

	// Compute Shader のコンパイル
	auto computeShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Skinning.CS.hlsl", L"cs_6_0");
	assert(computeShaderBlob);

	// RootSignature の作成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// 0: パレット (SRV)
	D3D12_DESCRIPTOR_RANGE rangePalette[1] = {};
	rangePalette[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangePalette[0].NumDescriptors = 1;
	rangePalette[0].BaseShaderRegister = 0; // t0
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = rangePalette;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	// 1: 入力頂点 (SRV)
	D3D12_DESCRIPTOR_RANGE rangeInput[1] = {};
	rangeInput[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangeInput[0].NumDescriptors = 1;
	rangeInput[0].BaseShaderRegister = 1; // t1
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = rangeInput;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	// 2: 出力頂点 (UAV)
	D3D12_DESCRIPTOR_RANGE rangeOutput[1] = {};
	rangeOutput[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	rangeOutput[0].NumDescriptors = 1;
	rangeOutput[0].BaseShaderRegister = 0; // u0
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = rangeOutput;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	// 3: スキニング情報 (CBV)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].Descriptor.ShaderRegister = 0; // b0

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		assert(false);
	}
	dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&skinningRootSignature_));

	// PipelineState の作成
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };
	computePipelineStateDesc.pRootSignature = skinningRootSignature_.Get();
	
	dxCommon_->GetDevice()->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&skinningPipelineState_));
}
