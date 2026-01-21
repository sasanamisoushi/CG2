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
#include"Model.h"
#include "ModelManager.h"
#include "Camera.h"

class Object3dCommon;

//struct Transform {
//	Vector3 scale;
//	Vector3 rotate;
//	Vector3 translate;
//};

//struct  VertexData {
//	Vector4 position;
//	Vector2 texcoord;
//	Vector3 normal; 
//};

//struct MaterialData {
//	std::string textureFilePath;
//	uint32_t textureIndex = 0;
//};
//
//struct ModelData {
//	std::vector<VertexData>vertices;
//	MaterialData material;
//};

//struct Material {
//	Vector4 color;
//	int32_t enableLighting;
//	float padding[3];
//	Matrix4x4 uvTransform;
//};

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

	

	

	//座標変換行列データ作成
	void CreateTransformationData();

	//平行光源データ作成
	void CreateDirectionLightData();

	Transform transform;
	//Transform cameraTransform;

	//セッター
	void SetModel(const std::string &filepath);
	void SetScale(const Vector3 &scale) { this->transform.scale = scale; }
	void SetRotate(const Vector3 &rotate) { this->transform.rotate = rotate; }
	void SetTranslate(const Vector3 &translate) { this->transform.translate = translate; }
	void SetCamera(Camera *camera) { this->camera = camera; }

	//ゲッター
	const Vector3 &GetScale()const { return transform.scale; }
	const Vector3 &GetRotate()const { return transform.rotate; }
	const Vector3 &GetTranslate()const { return transform.translate; }

private:

	Object3dCommon *object3dCommon = nullptr;

	
	MyMath *math = nullptr;

	//座標変換行列バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource;
	TransformationMatrix *transformationMatrixData = nullptr;

	//平行光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionLightResource;
	DirectionalLight *directionLightData = nullptr;

	

	bool useMonsterBall = true;


	//モデル
	Model *model = nullptr;

	//カメラ
	Camera *camera = nullptr;
	
};

