#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <random>
#include "DirectXCommon.h" 
#include "SrvManager.h"
#include "MyMath.h"
#include "Camera.h"


class ParticleManager {
public://構造体定義

	//パーティクル1粒のデータ構造
	struct Particle {
		Transform transform;
		Vector3 velocity;
		Vector4 color;
		float lifeTime;
		float currentTime;
	};

	// 頂点データ（ビルボード用の四角形を作るため）
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// GPUに送るデータ構造 (StructuredBuffer用)
	struct ParticleForGPU {
		Matrix4x4 WVP;    // World * View * Projection 行列 (画面上の座標決定用)
		Matrix4x4 World;  // World 行列 (本来の座標やライティング用)
		Vector4 color;    // 色 (フェードアウトさせるためのalpha値含む)
	};

	struct ParticleGroup {
		//マテリアルデータ(テクスチャファイルパスとティクスチャ用SRVインデックス)
		std::string textureFilePath;
		uint32_t textureSrvIndex = 0;
		//パーティクルのリスト(std::list<Particle>型)
		std::list<Particle> particle;
		//インスタンシングデータ用SRVインデックス
		uint32_t instancingSrvIndex;
		//インスタンシングリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
		//インスタンス数
		uint32_t kNumMaxInstance;
		// インスタンシングデータを書き込むためのポインタ
		ParticleForGPU *instancingDataPtr = nullptr;
	};

public://メンバ関数

	//初期化
	void Initialize(DirectXCommon *dxCommon);

	//更新
	void Update(Camera* camera);

	//描画
	void Draw();

	//パーティクルグループの生成
	void CreateParticleGroup(const std::string name, const std::string textureFilePath);

	void Emit(const std::string name, const Vector3 &position, uint32_t count);

private://メンバ関数
	//パイプライン生成関数
	void CreateRootSignature();
	void CreatePipelineState();

private://メンバ変数

	DirectXCommon *dxCommon_ = nullptr;


	//ランダムエンジン
	std::mt19937 randomEngine_;

	//頂点リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	//頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	//パイプライン関連
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	//パーティクル管理用リスト
	std::list<Particle> particles_;

	//パーティクルグループコンテナ
	std::unordered_map<std::string, ParticleGroup> particleGroups_;
};

