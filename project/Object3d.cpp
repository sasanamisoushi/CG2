#include "Object3d.h"
#include "Object3dCommon.h"
#include "TextureManager.h"
#include <fstream>
#include <sstream>
#include <string>
#include <wrl.h>

using Microsoft::WRL::ComPtr;






void Object3d::Initialize(Object3dCommon *object3dCommon) {
	//引数で受け取ってメンバ変数に記録する
	this->object3dCommon = object3dCommon;

	this->math = new MyMath();

	//モデルの読み込み
	modelData = LoadObjFile("resources", "plane.obj");

	//頂点データ作成
	CreateVertexData();

	//マテリアルデータ作成
	CreateMaterialData();

	//座標変換行列データ作成
	CreateTransformationData();

	//平行光源データ作成
	CreateDirectionLightData();

	//.objの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	//読み込んだテクスチャの番号を取得
	modelData.material.textureIndex = 
		TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData.material.textureFilePath);

	transform={ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	cameraTransform={ { 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,-10.0f } };
}

void Object3d::Update() {

	transform.rotate.y += 0.01f;

	

	Matrix4x4 worldMatrix = math->MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);


	//cameraMatrix
	Matrix4x4 cameraMatrix = math->MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);

	//viewMatrix
	Matrix4x4 viewMatrix = math->Inverse(cameraMatrix);

	//projectionMatrix
	Matrix4x4 projectionMatrix = math->MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClinentHeight), 0.1f, 100.0f);

	//worldViewProjectionMatrix
	//Matrix4x4 worldViewProjectionMatrix = math->Multiply(worldMatrix, math->Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData->WVP = worldMatrix* viewMatrix*projectionMatrix;
	transformationMatrixData->World = worldMatrix;

}

void Object3d::Draw() {

	

	object3dCommon->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	object3dCommon->GetDxCommon()->GetCommandList()->IASetIndexBuffer(nullptr);
	object3dCommon->GetDxCommon()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えて置けばよい
	object3dCommon->GetDxCommon()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//マテリアルCBufferの場所を設定
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, matetialResource->GetGPUVirtualAddress());

	//TransformationMatrixCbufferの場所を設定
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

	//SRVのDescriptorの先頭を設定
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureIndex));

	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionLightResource->GetGPUVirtualAddress());

	

	//描画
	object3dCommon->GetDxCommon()->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

	
}

MaterialData Object3d::LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename) {
	//構築するMaterialData
	MaterialData materialData;
	//ファイルから読んだ1行を格納するもの
	std::string line;
	//ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	//とりあえず開けなかったら止める
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		//identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			//連続してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

ModelData Object3d::LoadObjFile(const std::string &directoryPath, const std::string &filename) {
	ModelData modelData;
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals;  //法線
	std::vector<Vector2> texcoords; //テクスチャ座標
	std::string line;  //ファイルから読んだ1行を格納する

	std::ifstream file(directoryPath + "/" + filename);  //ファイルを開く
	assert(file.is_open()); //とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; //先頭の識別子を読む


		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {
			VertexData triangle[3];
			//面は三角形限定
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				//頂点の要素へのindexは「位置/uv/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');  //区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}

				//要素へのindexから、実際の要素の値を取得して頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				VertexData vertex = { position,texcord,normal };
				modelData.vertices.push_back(vertex);
				triangle[faceVertex] = { position,texcord,normal };
			}

			//頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			//matrialTemplateLidraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			//基本的にobjファイルと同一階層にmtlは存在されるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}

void Object3d::CreateVertexData() {

	//頂点リソースを作成
	vertexResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());

	

	////リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();

	//使用するリソースのサイズは頂点3つのサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	//1頂点当たりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	//頂点リソースデータを書き込む
	VertexData *vertexData = nullptr;

	//書き込む為のアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));

	//頂点データをリソースにコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

}

void Object3d::CreateMaterialData() {

	matetialResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(Material));

	//書き込むためのアドレスを取得
	matetialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	//今回は赤を書き込んで見る
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = true;
	materialData->uvTransform = math->MakeIdentity4x4();
}

void Object3d::CreateTransformationData() {
	//Plane用のWVPリソース
	wvpResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	
	wvpResource->Map(0, nullptr, reinterpret_cast<void **>(&transformationMatrixData));
	transformationMatrixData->WVP = math->MakeIdentity4x4();
	transformationMatrixData->World = math->MakeIdentity4x4();
}

void Object3d::CreateDirectionLightData() {
	//平行光源用のリソース
	directionLightResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));
	
	//書き込むためのアドレスを取得
	directionLightResource->Map(0, nullptr, reinterpret_cast<void **>(&directionLightData));

	directionLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionLightData->direction = { 1.0f,0.0f,0.0f };
	directionLightData->intensity = 1.0f;

	directionLightData->direction = math->Normalize(directionLightData->direction);

}
