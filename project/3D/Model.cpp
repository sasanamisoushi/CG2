#include "Model.h"
#include "engine/Resource/TextureManager.h"
#include <fstream>
#include <sstream>
#include <string>
#include <wrl.h>
#include <cmath>


using Microsoft::WRL::ComPtr;

void Model::Initialize(ModelCommon *modelCommon, const std::string &directorypath, const std::string &filename) {

	//引数で受け取ってメンバ変数に記録する
	this->modelCommon_ = modelCommon;

	//モデルの読み込み
	modelData = LoadObjFile(directorypath,filename);


	//頂点データ作成
	CreateVertexData();

	//マテリアルデータ作成
	CreateMaterialData();


	//.objの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	//読み込んだテクスチャの番号を取得
	modelData.material.textureIndex =
		TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData.material.textureFilePath);

	// テクスチャファイルパス
	//textureFilePath_= filename;

	textureFilePath_ = modelData.material.textureFilePath;
}


void Model::Draw() {
	SrvManager::GetInstance()->PreDraw();

	modelCommon_->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	//modelCommon_->GetDxCommon()->GetCommandList()->IASetIndexBuffer(nullptr);

	//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えて置けばよい
	modelCommon_->GetDxCommon()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//マテリアルCBufferの場所を設定
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, matetialResource->GetGPUVirtualAddress());

	//SRVのDescriptorの先頭を設定
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
	//描画
	modelCommon_->GetDxCommon()->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
}


void Model::CreateVertexData() {
	//頂点リソースを作成
	vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());



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


void Model::CreateMaterialData() {
	matetialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));

	//書き込むためのアドレスを取得
	matetialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	//今回は赤を書き込んで見る
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = 1;
	materialData->uvTransform = math->MakeIdentity4x4();

	materialData->shininess = 40.0f;
	materialData->alphaReference = modelData.material.alphaReference;
}

MaterialData Model::LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename) {
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

ModelData Model::LoadObjFile(const std::string &directoryPath, const std::string &filename) {
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
				VertexData vertex = { position,texcord,normal, {1.0f, 1.0f, 1.0f, 1.0f} };
				modelData.vertices.push_back(vertex);
				triangle[faceVertex] = vertex;
			}
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

void Model::InitializeSphere(ModelCommon *modelCommon, int subdivision) {
	// 引数で受け取ってメンバ変数に記録する
	this->modelCommon_ = modelCommon;

	// 球の頂点を計算するための変数
	const float pi = 3.1415926535f;
	const float latEvery = pi / subdivision;
	const float lonEvey = (2.0f * pi) / subdivision;

	std::vector<VertexData> tempVertices;

	// 球面上の頂点を計算
	for (int latIndex = 0; latIndex <= subdivision; ++latIndex) {
		float lat = -pi / 2.0f + latEvery * latIndex;
		for (int lonIndex = 0; lonIndex <= subdivision; ++lonIndex) {
			float lon = lonEvey * lonIndex;
			VertexData vertex{};

			// 法線と座標の計算
			vertex.normal.x = std::cos(lat) * std::cos(lon);
			vertex.normal.y = std::sin(lat);
			vertex.normal.z = std::cos(lat) * std::sin(lon);
			vertex.position = { vertex.normal.x,vertex.normal.y,vertex.normal.z,1.0f };

			// UV座標の計算
			vertex.texcoord = { float(lonIndex) / subdivision,1.0f - float(latIndex) / subdivision };

			// 頂点カラーのデフォルト
			vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			tempVertices.push_back(vertex);
		}
	}

	// 三角形リストとして　modelData.verticesに展開
	modelData.vertices.clear();
	for (int latIndex = 0; latIndex < subdivision; ++latIndex) {
		for (int lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
			int start = (latIndex * (subdivision + 1)) + lonIndex;

			int a = start;
			int b = start + 1;
			int c = start + (subdivision + 1);
			int d = start + (subdivision + 1) + 1;

			// 三角形1
			modelData.vertices.push_back(tempVertices[a]);
			modelData.vertices.push_back(tempVertices[c]);
			modelData.vertices.push_back(tempVertices[b]);

			// 三角形2
			modelData.vertices.push_back(tempVertices[b]);
			modelData.vertices.push_back(tempVertices[c]);
			modelData.vertices.push_back(tempVertices[d]);
		}
	}

	// 頂点データとマテリアルデータのバッファ作成
	CreateVertexData();
	CreateMaterialData();
}

void Model::InitializePlane(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

	// 平面を構成する4つの頂点（XY平面、幅2、高さ2）
	VertexData v[4];
	// 左下
	v[0].position = { -1.0f, -1.0f, 0.0f, 1.0f };
	v[0].texcoord = { 0.0f, 1.0f };
	v[0].normal = { 0.0f, 0.0f, -1.0f };
	v[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 左上
	v[1].position = { -1.0f, 1.0f, 0.0f, 1.0f };
	v[1].texcoord = { 0.0f, 0.0f };
	v[1].normal = { 0.0f, 0.0f, -1.0f };
	v[1].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 右下
	v[2].position = { 1.0f, -1.0f, 0.0f, 1.0f };
	v[2].texcoord = { 1.0f, 1.0f };
	v[2].normal = { 0.0f, 0.0f, -1.0f };
	v[2].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 右上
	v[3].position = { 1.0f, 1.0f, 0.0f, 1.0f };
	v[3].texcoord = { 1.0f, 0.0f };
	v[3].normal = { 0.0f, 0.0f, -1.0f };
	v[3].color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 三角形リストとして展開（インデックスを使わない直書き）
	// 三角形1（左下、左上、右下）
	modelData.vertices.push_back(v[0]);
	modelData.vertices.push_back(v[1]);
	modelData.vertices.push_back(v[2]);
	// 三角形2（左上、右上、右下）
	modelData.vertices.push_back(v[1]);
	modelData.vertices.push_back(v[3]);
	modelData.vertices.push_back(v[2]);

	// バッファ作成
	CreateVertexData();
	CreateMaterialData();

	// テクスチャの設定
	textureFilePath_ = "resources/uvChecker.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

void Model::InitializeBox(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

	// 箱の角になる8つの頂点座標（幅2、高さ2、奥行き2）
	Vector4 p[8] = {
		{-1.0f, -1.0f, -1.0f, 1.0f}, // 0: 左下・手前
		{-1.0f,  1.0f, -1.0f, 1.0f}, // 1: 左上・手前
		{ 1.0f, -1.0f, -1.0f, 1.0f}, // 2: 右下・手前
		{ 1.0f,  1.0f, -1.0f, 1.0f}, // 3: 右上・手前
		{-1.0f, -1.0f,  1.0f, 1.0f}, // 4: 左下・奥
		{-1.0f,  1.0f,  1.0f, 1.0f}, // 5: 左上・奥
		{ 1.0f, -1.0f,  1.0f, 1.0f}, // 6: 右下・奥
		{ 1.0f,  1.0f,  1.0f, 1.0f}  // 7: 右上・奥
	};

	// 6つの面が向いている方向（法線）
	Vector3 nFront = { 0.0f,  0.0f, -1.0f };
	Vector3 nBack = { 0.0f,  0.0f,  1.0f };
	Vector3 nLeft = { -1.0f,  0.0f,  0.0f };
	Vector3 nRight = { 1.0f,  0.0f,  0.0f };
	Vector3 nTop = { 0.0f,  1.0f,  0.0f };
	Vector3 nBottom = { 0.0f, -1.0f,  0.0f };

	// テクスチャのUV座標
	Vector2 uv00 = { 0.0f, 1.0f };
	Vector2 uv01 = { 0.0f, 0.0f };
	Vector2 uv10 = { 1.0f, 1.0f };
	Vector2 uv11 = { 1.0f, 0.0f };

	// 面（四角形＝三角形2枚）を作る便利関数
	auto addFace = [&](int i0, int i1, int i2, int i3, Vector3 n) {
		Vector4 c = { 1.0f, 1.0f, 1.0f, 1.0f };
		modelData.vertices.push_back({ p[i0], uv00, n, c });
		modelData.vertices.push_back({ p[i1], uv01, n, c });
		modelData.vertices.push_back({ p[i2], uv10, n, c });

		modelData.vertices.push_back({ p[i1], uv01, n, c });
		modelData.vertices.push_back({ p[i3], uv11, n, c });
		modelData.vertices.push_back({ p[i2], uv10, n, c });
		};

	// 前, 奥, 左, 右, 上, 下 の6面を作る
	addFace(0, 1, 2, 3, nFront);
	addFace(6, 7, 4, 5, nBack);
	addFace(4, 5, 0, 1, nLeft);
	addFace(2, 3, 6, 7, nRight);
	addFace(1, 5, 3, 7, nTop);
	addFace(4, 0, 6, 2, nBottom);

	// バッファ作成
	CreateVertexData();
	CreateMaterialData();

	// テクスチャの設定
	textureFilePath_ = "resources/uvChecker.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

void Model::InitializeRing(ModelCommon *modelCommon, int subdivision, float outerRadius, float innerRadius,
	bool isUvHorizontal, const Vector4& innerColor, const Vector4& outerColor,
	float startAngleDegree, float endAngleDegree, float fadeAngleDegree) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

	const float pi = 3.1415926535f;
	
	// 角度の補正（終了角度が開始角度以下なら360度足すなど）
	if (endAngleDegree <= startAngleDegree) endAngleDegree += 360.0f;
	
	float startRad = startAngleDegree * pi / 180.0f;
	float endRad = endAngleDegree * pi / 180.0f;
	float fadeRad = fadeAngleDegree * pi / 180.0f;
	float totalRad = endRad - startRad;

	std::vector<VertexData> tempVertices;

	// リングの頂点を計算
	for (int i = 0; i <= subdivision; ++i) {
		float t = (float)i / subdivision;
		float theta = startRad + t * totalRad;
		float c = std::cos(theta);
		float s = std::sin(theta);
		
		// アルファフェードの計算
		float alpha = 1.0f;
		if (fadeRad > 0.0f) {
			float distFromStart = theta - startRad;
			float distFromEnd = endRad - theta;
			float minEdgeDist = (std::min)(distFromStart, distFromEnd);
			if (minEdgeDist < fadeRad) {
				alpha = minEdgeDist / fadeRad;
			}
		}

		Vector4 outColor = { outerColor.x, outerColor.y, outerColor.z, outerColor.w * alpha };
		Vector4 inColor = { innerColor.x, innerColor.y, innerColor.z, innerColor.w * alpha };

		// テクスチャ座標の計算
		float uv_u = isUvHorizontal ? t : 0.0f;
		float uv_v = isUvHorizontal ? 0.0f : t;

		// 外側の頂点
		VertexData vOuter;
		vOuter.position = { outerRadius * c, outerRadius * s, 0.0f, 1.0f };
		vOuter.texcoord = { uv_u, uv_v };
		vOuter.normal = { 0.0f, 0.0f, -1.0f };
		vOuter.color = outColor;

		// 内側の頂点
		VertexData vInner;
		vInner.position = { innerRadius * c, innerRadius * s, 0.0f, 1.0f };
		vInner.texcoord = { isUvHorizontal ? t : 1.0f, isUvHorizontal ? 1.0f : t };
		vInner.normal = { 0.0f, 0.0f, -1.0f };
		vInner.color = inColor;

		tempVertices.push_back(vOuter); // index 2*i
		tempVertices.push_back(vInner); // index 2*i + 1
	}

	// 三角形リストとして展開
	// 前面から見て時計回り（左手座標系の標準）になるように頂点順序を設定
	for (int i = 0; i < subdivision; ++i) {
		int outer0 = 2 * i;
		int inner0 = 2 * i + 1;
		int outer1 = 2 * (i + 1);
		int inner1 = 2 * (i + 1) + 1;

		// 三角形1: outer0 -> inner0 -> inner1
		modelData.vertices.push_back(tempVertices[outer0]);
		modelData.vertices.push_back(tempVertices[inner0]);
		modelData.vertices.push_back(tempVertices[inner1]);

		// 三角形2: outer0 -> inner1 -> outer1
		modelData.vertices.push_back(tempVertices[outer0]);
		modelData.vertices.push_back(tempVertices[inner1]);
		modelData.vertices.push_back(tempVertices[outer1]);
	}

	// 頂点データとマテリアルデータのバッファ作成
	CreateVertexData();
	CreateMaterialData();

	// テクスチャの設定
	textureFilePath_ = "resources/gradationLine.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

void Model::InitializeCylinder(ModelCommon *modelCommon,
	int subdivision, int verticalSubdivision,
	float topRadiusX, float topRadiusZ,
	float bottomRadiusX, float bottomRadiusZ,
	float height,
	const Vector4& topColor, const Vector4& bottomColor,
	float startAngleDegree, float endAngleDegree,
	bool isUvFlipped) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

	const float pi = 3.1415926535f;
	if (endAngleDegree <= startAngleDegree) endAngleDegree += 360.0f;

	float startRad = startAngleDegree * pi / 180.0f;
	float endRad = endAngleDegree * pi / 180.0f;
	float totalRad = endRad - startRad;

	// 頂点格子を生成
	std::vector<std::vector<VertexData>> grid(verticalSubdivision + 1, std::vector<VertexData>(subdivision + 1));

	for (int v = 0; v <= verticalSubdivision; ++v) {
		float vt = (float)v / verticalSubdivision;
		float y = height * vt;
		float radiusX = bottomRadiusX + (topRadiusX - bottomRadiusX) * vt;
		float radiusZ = bottomRadiusZ + (topRadiusZ - bottomRadiusZ) * vt;
		Vector4 color = {
			bottomColor.x + (topColor.x - bottomColor.x) * vt,
			bottomColor.y + (topColor.y - bottomColor.y) * vt,
			bottomColor.z + (topColor.z - bottomColor.z) * vt,
			bottomColor.w + (topColor.w - bottomColor.w) * vt
		};

		for (int h = 0; h <= subdivision; ++h) {
			float ht = (float)h / subdivision;
			float theta = startRad + ht * totalRad;
			float sin = std::sin(theta);
			float cos = std::cos(theta);

			VertexData vertex;
			vertex.position = { -sin * radiusX, y, cos * radiusZ, 1.0f };

			float u = ht;
			float v_coord = isUvFlipped ? vt : (1.0f - vt);
			vertex.texcoord = { u, v_coord };

			// 法線は外向き
			vertex.normal = { -sin, 0.0f, cos };
			vertex.color = color;

			grid[v][h] = vertex;
		}
	}

	// 三角形リストとして展開
	for (int v = 0; v < verticalSubdivision; ++v) {
		for (int h = 0; h < subdivision; ++h) {
			const VertexData& v0 = grid[v][h];
			const VertexData& v1 = grid[v + 1][h];
			const VertexData& v2 = grid[v][h + 1];
			const VertexData& v3 = grid[v + 1][h + 1];

			// 三角形1: v0 -> v1 -> v3
			modelData.vertices.push_back(v0);
			modelData.vertices.push_back(v3);
			modelData.vertices.push_back(v1);

			// 三角形2: v0 -> v3 -> v2
			modelData.vertices.push_back(v0);
			modelData.vertices.push_back(v2);
			modelData.vertices.push_back(v3);
		}
	}

	// 頂点データとマテリアルデータのバッファ作成
	CreateVertexData();
	CreateMaterialData();

	// デフォルトでライティングを無効化
	if (materialData) {
		materialData->enableLighting = 0;
	}

	// テクスチャの設定
	textureFilePath_ = "resources/gradationLine.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}
