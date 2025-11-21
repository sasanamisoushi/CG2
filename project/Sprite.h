#pragma once
#include "MyMath.h"
#include <cstdint>
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

using Microsoft::WRL::ComPtr;


class SpriteCommon;

class Sprite {
public:
	//初期化
	void Initialize(SpriteCommon *spriteCommon);

	//更新処理
	void Update();

	//描画
	void Draw();

	//頂点データ
	struct VertexData {
		Vector4 Position;
		Vector2 Texcoord;
		Vector3 normal;
	};

	//マテリアルデータ
	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	//座標変換秒列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};
	
	struct Transform {
		Vector3 scale;
		Vector3 rotate;
		Vector3 translate;
	};

private:
	SpriteCommon *spriteCommon=nullptr;
	MyMath *math = nullptr;

	// 頂点バッファ
	ComPtr<ID3D12Resource> vertexResource;

	// インデックスバッファ
	ComPtr<ID3D12Resource> indexResource;

	//バッファリソース内のデータを指するポインタ
	VertexData *vertexData_ = nullptr;
	uint32_t *indexData_ = nullptr;

	//バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	//頂点データ作成
	void CreateVertexData();

	//マテリアルリソースを作る
	ComPtr<ID3D12Resource> materialResource;
	//マテリアルにデータを書き込む
	Material *materialData = nullptr;

	//マテリアルデータの作成
	void CreateMaterialData();

	//Sprite用のTransformationMatrix用のリソースをス来る
	ComPtr<ID3D12Resource> transformationMatrixResource;
	//データを書き込む
	TransformationMatrix *transformetionMatrixData = nullptr;

	//座標変換行列データ作成
	void CreateTransformationData();

	
};

