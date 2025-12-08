#include "Object3dCommon.h"
#include <externals/DirectXTex/d3dx12.h>
#include <cassert>

void Object3dCommon::Initialize(DirectXCommon *dxCommon) {
	//引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

	//シェーダーの読み込み
	LoadShaders();

	//グラフィックスパイプラインの生成
	CreateGraphicsPipeline();
}

void Object3dCommon::SetCommonDrawSettings() {
	//ルートシグネチャをセットするコマンド
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	//グラフィックスパイプラインをセットするコマンド
	dxCommon_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	//プリミティブポロジーをセットするコマンド
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::LoadShaders() {
	//Shaderをコンパイルする
	vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3D.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3D.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

}

void Object3dCommon::CreateRootSignature() {
	// ディスクリプタレンジ
	// t0: テクスチャ (SRV) 用
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成 (4つ定義)
	D3D12_ROOT_PARAMETER rootParamerers[4] = {};

	// 0: マテリアルCBV (b0, PixelShader)
	rootParamerers[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[0].Descriptor.ShaderRegister = 0;

	// 1: WVP/World行列CBV (b0, VertexShader)
	rootParamerers[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParamerers[1].Descriptor.ShaderRegister = 0;

	// 2: テクスチャDescriptorTable (t0, PixelShader)
	rootParamerers[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParamerers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParamerers[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// 3: 平行光源CBV (b1, PixelShader)
	rootParamerers[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[3].Descriptor.ShaderRegister = 1;

	descriptionRootSignature.pParameters = rootParamerers;
	descriptionRootSignature.NumParameters = _countof(rootParamerers);


	// スタティックサンプラーの定義
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズと生成
	ID3DBlob *signatureBlob = nullptr;
	ID3DBlob *errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		//logger.Log( reinterpret_cast<char *>(errorBlob->GetBufferPointer())); // loggerがない場合はassertで代用
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateGraphicsPipeline() {

	// シェーダーロードとルートシグネチャ作成を前提
	CreateRootSignature();

	// InputLayoutの作成 (SpriteCommonと同じ内容でOK)
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの作成 (アルファブレンドは無効/デフォルト)
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerStateの作成
	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	// 3Dでは裏面を非表示にする (背面カリング) を有効にする
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みを許可
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual (手前にあるものを描画)
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// ステンシルは今回は使用しないためデフォルト

	// PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};

	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;

	// シェーダーはObject3D用にロードされたものを使用 (LoadShaders()を別途実装しているはず)
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	pipelineState_ = nullptr;
	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));

}
