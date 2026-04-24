#pragma once
#include "ModelCommon.h"
#include "MyMath.h"
#include <memory>


struct  VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
	Vector4 color;
};

struct MaterialData {
	std::string textureFilePath;
	uint32_t textureIndex = 0;
};

struct ModelData {
	std::vector<VertexData>vertices;
	MaterialData material;
};

struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
	float shininess;
};

class Model {
public:

	//初期化
	void Initialize(ModelCommon *modelCommon,const std::string& directorypath,const std::string& filename);

	//描画
	void Draw();

	//頂点データ作成
	void CreateVertexData();

	//マテリアルデータ作成
	void CreateMaterialData();

	//.mtlファイルの読み込み
	static MaterialData LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename);

	//.objファイルの読み取り
	static ModelData LoadObjFile(const std::string &directoryPath, const std::string &filename);

	// 球の初期化
	void InitializeSphere(ModelCommon *modelCommon, int subdivision);

	// 平面の初期化
	void InitializePlane(ModelCommon *modelCommon);

	// ボックスの初期化
	void InitializeBox(ModelCommon *modelCommon);

	// リングの初期化
	void InitializeRing(ModelCommon *modelCommon, int subdivision = 32, float outerRadius = 1.0f, float innerRadius = 0.5f,
		bool isUvHorizontal = true, const Vector4& innerColor = { 1.0f, 1.0f, 1.0f, 1.0f }, const Vector4& outerColor = { 1.0f, 1.0f, 1.0f, 1.0f },
		float startAngleDegree = 0.0f, float endAngleDegree = 360.0f, float fadeAngleDegree = 0.0f);

	void SetUvTransform(const Matrix4x4& transform) {
		if (materialData) {
			materialData->uvTransform = transform;
		}
	}

	ModelCommon* GetModelCommon() const { return modelCommon_; }

private:
	ModelCommon *modelCommon_;

	std::unique_ptr<MyMath> math = std::make_unique<MyMath>();

	//Objファイルのデータ
	ModelData modelData;

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	//バッファリソース内のデータを指すポインタ
	VertexData *vertexData = nullptr;
	//バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	//マテリアルリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> matetialResource;
	//バッファリソース内のデータを指すポイント
	Material *materialData = nullptr;

	// テクスチャファイルパス
	std::string textureFilePath_;

};

