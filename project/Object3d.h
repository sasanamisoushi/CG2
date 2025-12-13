#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cassert>
#include <MyMath.h>

class Object3dCommon;

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct  VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal; 
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
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLight {
	Vector4 color; //ライトの色
	Vector3 direction;
	float intensity;
};

class Object3d {
public: 

	

	//初期化
	void Initialize(Object3dCommon *object3dCommon);

	//更新
	void Update();

	//描画
	void Draw();

	//.mtlファイルの読み込み
	static MaterialData LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename);

	//.objファイルの読み取り
	static ModelData LoadObjFile(const std::string &directoryPath, const std::string &filename);

	//頂点データ作成
	void CreateVertexData();

	//マテリアルデータ作成
	void CreateMaterialData();

	//座標変換行列データ作成
	void CreateTransformationData();

	//平行光源データ作成
	void CreateDirectionLightData();

	Transform transform;
	Transform cameraTransform;

private:

	Object3dCommon *object3dCommon = nullptr;

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

	MyMath *math = nullptr;

	//座標変換行列バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource;
	TransformationMatrix *transformationMatrixData = nullptr;

	//平行光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionLightResource;
	DirectionalLight *directionLightData = nullptr;

	

	bool useMonsterBall = true;

	
	
};

