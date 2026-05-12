#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <random>
#include "engine/Graphics/DirectXCommon.h" 
#include "engine/Graphics/SrvManager.h"
#include "engine/Math/MyMath.h"
#include "engine/Camera/Camera.h"


class ParticleManager {
public://構造体定義

	//パーティクル1粒のデータ構造 (CPU用)
	struct Particle {
		EulerTransform transform;
		Vector3 velocity;
		Vector4 color;
		float lifeTime;
		float currentTime;
	};

	// パーティクル1粒のデータ構造 (GPU用)
	struct ParticleCS {
		Vector3 translate;
		Vector3 scale;
		float lifeTime;
		Vector3 velocity;
		float currentTime;
		Vector4 color;
	};

	// 頂点データ（ビルボード用の四角形を作るため）
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct ParticleForGPU {
		Matrix4x4 WVP;    // World * View * Projection 行列 (画面上の座標決定用)
		Matrix4x4 World;  // World 行列 (本来の座標やライティング用)
		Vector4 color;    // 色 (フェードアウトさせるためのalpha値含む)
	};

	// カメラデータ (GPU用)
	struct CameraForGPU {
		Matrix4x4 viewProjection;
		Matrix4x4 billboard;
	};

	// GPU初期化用パラメータ
	struct ParticleGPUParam {
		Vector3 translate;
		float pad1;
		Vector3 scale;
		float pad2;
		Vector4 color;
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

		// --- GPU Particle 追加分 ---
		Microsoft::WRL::ComPtr<ID3D12Resource> particleResource;
		uint32_t uavIndex;
		uint32_t srvIndex;
		bool isGpuInitialized = false;
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

	// getter/setter
	Vector3& GetGpuParticleTranslate() { return gpuParticleTranslate; }
	Vector3& GetGpuParticleScale() { return gpuParticleScale; }
	Vector4& GetGpuParticleColor() { return gpuParticleColor; }
	void RequestGpuInitialize() { requestGpuInitialize = true; }

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

	// --- GPU Particle 追加分 ---
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;
	void CreateComputeRootSignature();
	void CreateComputePipelineState();

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU *cameraDataPtr_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> gpuParamResource_;
	ParticleGPUParam *gpuParamDataPtr_ = nullptr;

	// --- GPU Particle 操作用 ---
	Vector3 gpuParticleTranslate = { 0.0f, 0.0f, 0.0f };
	Vector3 gpuParticleScale = { 1.0f, 1.0f, 1.0f };
	Vector4 gpuParticleColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool requestGpuInitialize = false;

	//パーティクル管理用リスト
	std::list<Particle> particles_;

	//パーティクルグループコンテナ
	std::unordered_map<std::string, ParticleGroup> particleGroups_;
};

