#pragma once
#include <vector>
#include <MyMath.h>

class Object3dCommon;

struct  VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal; //05_03で追加
};

struct MaterialData {
	std::string textureFilePath;
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

class Object3d {
public: 

	

	//初期化
	void Initialize(Object3dCommon *object3dCommon);

	//.mtlファイルの読み込み
	static MaterialData LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename);

	//.objファイルの読み取り
	static ModelData LoadObjFile(const std::string &directoryPath, const std::string &filename);

	//頂点データ作成
	void CreateVertexData();

	//マテリアルデータ作成
	void CreateMaterialData();

	//座標変換行列
	void CreateTransformationData();

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

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourcePlane;
	TransformationMatrix *transformationMatrixData = nullptr;

};

