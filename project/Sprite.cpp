#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"




void Sprite::Initialize(SpriteCommon *spriteCommon, std::string textureFilePath) {
	//引数で受け取ってメンバ変数に記録する
	this->spriteCommon = spriteCommon;

	math = new MyMath();

	//頂点データ作成
	CreateVertexData();

	//マテリアルデータの作成
	CreateMaterialData();

	//座標変換行列データ作成
	CreateTransformationData();

	//単位行列を書き込んでいく
	//TextureManager::GetInstance()->LoadTexture("circle.png"); // ロード処理を追加
	textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	textureFilePath_ = textureFilePath;

	//テクスチャサイズをイメージに合わせる
	AdjustTextureSize();
}

void Sprite::Update() {

	float left = 0.0f - anchorPoint.x;
	float right = 1.0f - anchorPoint.x;
	float top = 0.0f - anchorPoint.y;
	float bottom = 1.0f - anchorPoint.y;

	//左右反転
	if (isFlipX_) {
		left = -left;
		right = -right;
	}

	//上下反転
	if (isFlipY_) {
		top = -top;
		bottom = -bottom;
	}

	const DirectX::TexMetadata &metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath_);
	float tex_left = textureLeftTop.x / metadata.width;
	float tex_rigth = (textureLeftTop.x + textureSize.x) / metadata.width;
	float tex_top = textureLeftTop.y / metadata.height;
	float tex_bottom = (textureLeftTop.y + textureSize.y) / metadata.height;

	//頂点リソースにデータを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData_));

	//1枚目の三角形
	vertexData_[0].Position= { left,bottom,0.0f,1.0f };
	vertexData_[0].Texcoord = { tex_left,tex_bottom };
	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[1].Position = { left,top,0.0f,1.0f };
	vertexData_[1].Texcoord = { tex_left,tex_top };
	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[2].Position = { right,bottom,0.0f,1.0f };
	vertexData_[2].Texcoord = { tex_rigth,tex_bottom };
	vertexData_[2].normal = { 0.0f,0.0f,-1.0f };

	//2枚目の三角形
	vertexData_[3].Position = { right,top,0.0f,1.0f };
	vertexData_[3].Texcoord = { tex_rigth,tex_top };
	vertexData_[3].normal = { 0.0f,0.0f,-1.0f };

	//インデックスリソースにデータを書き込む
	indexResource->Map(0, nullptr, reinterpret_cast<void **>(&indexData_));
	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2;
	indexData_[3] = 1;
	indexData_[4] = 3;
	indexData_[5] = 2;

	//Transform情報を作る
	Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	transform.translate = { position.x,position.y,0.0f };
	transform.rotate = { 0.0f,0.0f,rotation };
	transform.scale = { size.x,size.y,1.0f };

	//TransformからWorldMatrixを作る
	Matrix4x4 worldMatrix = math->MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	//ViewMatrixを作って単位行列を代入
	Matrix4x4 viewMatrix = math->MakeIdentity4x4();
	//ProjectionMatrixを作って並行投影行列を書き込む
	Matrix4x4 projectionMatrix = math->MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClinentHeight), 0.0f, 100.0f);

	transformetionMatrixData->WVP = math->Multiply(worldMatrix, math->Multiply(viewMatrix, projectionMatrix));
	transformetionMatrixData->World = worldMatrix;
}

void Sprite::Draw() {

	

	//マテリアルCBufferの場所を設定
	spriteCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());


	//Supriteの描画。
	spriteCommon->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	spriteCommon->GetDxCommon()->GetCommandList()->IASetIndexBuffer(&indexBufferView);

	//TransformationMatrixCbufferの場所を設定
	spriteCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());

	//SRVのDescriptorの先頭を設定
	spriteCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

	//描画
	spriteCommon->GetDxCommon()->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

}

void Sprite::textureReplacement(std::string textureFilePath) {
	// テクスチャをロードする
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	// 差し替えたいテクスチャのインデックスを取得する
	uint32_t newIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
	// 自身のテクスチャインデックスを更新する
	this->textureIndex = newIndex;
}

Sprite::~Sprite() {

	delete math;
}

void Sprite::CreateVertexData() {

	//Sprite用の頂点リソースを作る
	vertexResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 6);

	//Index用の頂点リソースを作る
	indexResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);
	
	

	//リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();

	//使用するリソースのサイズは頂点６つ分のサイズ
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 6;

	//1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	
	//リソースの先頭のアドレスから使う
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}

void Sprite::CreateMaterialData() {

	//Sprite用のマテリアルリソースを作る
	materialResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(Material));
	

	//書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	//白で設定
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->uvTransform = math->MakeIdentity4x4();
}

void Sprite::CreateTransformationData() {

	//TransformationMatrix用のリソースをス来る
	transformationMatrixResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	//書き込むためのアドレスを取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void **>(&transformetionMatrixData));
	//単位行列を書き込んでおく
	transformetionMatrixData->WVP = math->MakeIdentity4x4();
	transformetionMatrixData->World = math->MakeIdentity4x4();


}

void Sprite::AdjustTextureSize() {

	//テクスチャメタデータを取得
	const DirectX::TexMetadata &metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath_);

	textureSize.x = static_cast<float>(metadata.width);
	textureSize.y = static_cast<float>(metadata.height);

	//画像サイズをテクスチャサイズに合わせる
	size = textureSize;
}
