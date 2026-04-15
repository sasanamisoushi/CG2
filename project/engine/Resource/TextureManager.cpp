#include "TextureManager.h"
#include <engine/Graphics/DirectXCommon.h>
#include <filesystem>



TextureManager *TextureManager::instance = nullptr;

//ImGuiで0番を仕様するため、1番から使用
uint32_t TextureManager::kSRVIndexTop = 1;

TextureManager *TextureManager::GetInstance() {
	if (instance == nullptr) {
		instance = new TextureManager;
	}
	return instance;
}

void TextureManager::Finalize() {
	delete instance;
	instance = nullptr;
}

void TextureManager::Initialize(DirectXCommon *common,SrvManager *srvManager) {
	//SRVの数と同数
	textureDatas.reserve(DirectXCommon::kMaxSRVCount);
	dxCommon = common;

	this->srvManager = srvManager;

}

void TextureManager::LoadTexture(const std::string &filePath) {

	if (textureDatas.contains(filePath)) {
		return;
	}

	
	//テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = su.ConvertString(filePath);
	HRESULT hr = S_FALSE;
		
	// 拡張子によって読み込み処理を分岐する
	std::filesystem::path path(filePath);
	if (path.extension() == ".dds") {
		// スカイボックスなどで使うDDｓファイルの読み込み
		hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
	} else {
		// 従来のPNGやJPGなどのWICファイルの読み込み
		hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	}
	assert(SUCCEEDED(hr));


	//ミップマップの作成(DDS等の圧縮フォーマット時はスキップする対応)
	DirectX::ScratchImage mipImages{};
	if (DirectX::IsCompressed(image.GetMetadata().format)) {
		mipImages = std::move(image);
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
		assert(SUCCEEDED(hr));
	}

	//追加したテクスチャデータの参照を取得する
	TextureData &textureData = textureDatas[filePath];
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon->CreateTextureResource(textureData.metadata);

	//テクスチャデータの要素数番号をSRVのインデックスとする
	uint32_t srvIndex = static_cast<uint32_t>(textureDatas.size() - 1) + kSRVIndexTop;
	textureData.srvIndex = srvManager->Allocate();
	textureData.srvHandleCPU = srvManager->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager->GetGPUDescriptorHandle(textureData.srvIndex);

	//転送用に生成した中間リソースをテクスチャデータ構造体に格納
	textureData.intermediateResource = dxCommon->UploadTextureData(textureData.resource, mipImages);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	
	// メタデータからキューブマップかどうか判定してSRVの次元を変える
	if (textureData.metadata.IsCubemap()) {
		// スカイボックス用のキューブマップテクスチャとして設定
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT(textureData.metadata.mipLevels);
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else {
		// 通常の2Dテクスチャとして設定
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);
	}

	
	//SRVの生成
	dxCommon->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string &filePath) {
	//読み込み済みテクスチャデータを検索
	//auto it = std::find_if(
	//	textureDatas.begin(), 
	//	textureDatas.end(), 
	//	[&](TextureData &textureData) {return textureData.filePath == filePath; }
	//);
	//if (it != textureDatas.end()) {
	//	//読み込み済みなら要素番号を返す
	//	uint32_t textureIndex = static_cast<uint32_t>(std::distance(textureDatas.begin(), it));
	//	return textureIndex;
	//}

	assert(textureDatas.contains(filePath));
	return textureDatas.at(filePath).srvIndex;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string &filePath) {

	//範囲指定違反チェック
	assert(textureDatas.contains(filePath));

	// テクスチャデータの参照を取得
	TextureData &textureData = textureDatas[filePath];
	return textureData.srvHandleGPU;
}

const DirectX::TexMetadata &TextureManager::GetMetaData(const std::string &filePath) {
	
	//範囲外指定違反チェック
	assert(textureDatas.contains(filePath));

	//テクスチャデータの参照を取得
	TextureData &textureData = textureDatas[filePath];
	return textureData.metadata;
}
