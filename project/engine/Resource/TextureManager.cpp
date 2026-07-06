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

	DirectX::ScratchImage image{};
	DirectX::ScratchImage mipImages{};
	std::wstring filePathW = su.ConvertString(filePath);
	HRESULT hr = E_FAIL;
		
	if (filePath == "resources/white1x1.png") {
		hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1, 1, 1, 1);
		if (SUCCEEDED(hr)) {
			uint8_t* pixels = image.GetPixels();
			if (pixels) {
				pixels[0] = 0xFF; // R
				pixels[1] = 0xFF; // G
				pixels[2] = 0xFF; // B
				pixels[3] = 0xFF; // A
			}
			mipImages = std::move(image);
		}
	} else {
		try {
			std::filesystem::path path(filePath);
			if (path.extension() == ".dds") {
				hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
			} else {
				hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
			}

			if (SUCCEEDED(hr)) {
				if (DirectX::IsCompressed(image.GetMetadata().format)) {
					mipImages = std::move(image);
				} else {
					hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
					if (FAILED(hr)) {
						mipImages = std::move(image);
					}
				}
			}
		} catch (const std::exception& e) {
			char buf[512];
			sprintf_s(buf, "Exception in LoadTexture for %s: %s\n", filePath.c_str(), e.what());
			OutputDebugStringA(buf);
			hr = E_FAIL;
		} catch (...) {
			char buf[512];
			sprintf_s(buf, "Unknown exception in LoadTexture for %s\n", filePath.c_str());
			OutputDebugStringA(buf);
			hr = E_FAIL;
		}
	}

	if (FAILED(hr) || !mipImages.GetImages()) {
		char buf[1024];
		std::filesystem::path absolutePath = std::filesystem::absolute(filePath);
		sprintf_s(buf, "[TextureManager] Failed to load texture: %s, HRESULT: 0x%08X, Absolute Path: %s\n", 
			filePath.c_str(), (unsigned int)hr, absolutePath.string().c_str());
		OutputDebugStringA(buf);

		std::string fallbackPath = "resources/white1x1.png";
		if (filePath != fallbackPath) {
			LoadTexture(fallbackPath);
			if (textureDatas.contains(fallbackPath)) {
				textureDatas[filePath] = textureDatas[fallbackPath];
				return;
			}
		}
		return;
	}

	TextureData textureData;
	textureData.filePath = filePath;
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon->CreateTextureResource(textureData.metadata);

	if (textureData.resource == nullptr) {
		std::string fallbackPath = "resources/white1x1.png";
		if (filePath != fallbackPath) {
			LoadTexture(fallbackPath);
			if (textureDatas.contains(fallbackPath)) {
				textureDatas[filePath] = textureDatas[fallbackPath];
				return;
			}
		}
		std::string errorMsg = "Fatal: CreateTextureResource failed for texture: " + filePath;
		OutputDebugStringA((errorMsg + "\n").c_str());
		return;
	}

	textureData.srvIndex = srvManager->Allocate();
	textureData.srvHandleCPU = srvManager->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager->GetGPUDescriptorHandle(textureData.srvIndex);

	textureData.intermediateResource = dxCommon->UploadTextureData(textureData.resource, mipImages);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	
	if (textureData.metadata.IsCubemap()) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT(textureData.metadata.mipLevels);
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);
	}

	dxCommon->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

	textureDatas[filePath] = std::move(textureData);
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

void TextureManager::SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList *commandList, UINT rootParameterIndex, const std::string &filePath) {
	// 指定されたファイルパスのテクスチャが読み込まれているかチェック
	assert(textureDatas.contains(filePath) && "Texture is not loaded.");

	// GPUハンドルを取得して、コマンドリストの指定インデックスにセットする
	commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, textureDatas.at(filePath).srvHandleGPU);
}
