#pragma once
#include <string>
#include "DirectXCollision.h"
#include "StringUtility.h"
#include <externals/DirectXTex/DirectXTex.h>
#include <externals/DirectXTex/d3dx12.h>
#include <unordered_map>
#include "SrvManager.h"

class DirectXCommon;

class TextureManager {
public:
	//シングルトンインスタンスの取得
	static TextureManager *GetInstance();

	//終了
	void Finalize();

	//初期化
	void Initialize(DirectXCommon *common,SrvManager* srvManager);

	//テクスチャファイルの読み込み
	void LoadTexture(const std::string &filePath);

	//SRVインデックスの開始番号
	uint32_t GetTextureIndexByFilePath(const std::string &filePath);

	//テクスチャ番号からGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string &filePath);

	//メタデータの取得
	const DirectX::TexMetadata &GetMetaData(const std::string& filePath);
	

private:
	static TextureManager *instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager &) = delete;
	TextureManager &operator=(TextureManager &) = delete;

	//テクスチャ1枚分のデータ
	struct TextureData {
		std::string filePath;                              //画像ファイル
		DirectX::TexMetadata metadata;                     //画像の幅や高さなどの情報
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;   //テクスチャリソース
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;          //SRV作成時に必要なCPUハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;          //描画コマンドに必要なGPUハンドル
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
	};

	//テクスチャデータ
	std::unordered_map<std::string,TextureData> textureDatas;

	DirectXCommon *dxCommon = nullptr;

	StringUtility su;

	//SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

	SrvManager *srvManager = nullptr;
	
};

