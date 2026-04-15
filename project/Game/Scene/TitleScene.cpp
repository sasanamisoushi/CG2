#include "TitleScene.h"
#include "3D/Object3dCommon.h"
#include "engine/Input/Input.h"
#include "SceneManager.h"

void TitleScene::Initialize() {
	//カメラ・シーンリソース
	camera = std::make_unique<Camera>();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	//モデル
	ModelManager::GetInstance()->LoadModel("plane.obj");

	//オブジェクト
	objA = std::make_unique<Object3d>();
	objA->Initialize(Object3dCommon::GetInstance());
	objA->SetModel("plane.obj");
	objA->transform.translate = { -2.0f,0.0f,0.0f };
	objects.push_back(objA.get());
}

void TitleScene::Finalize() {

}

void TitleScene::Update() {

	//スペースキーが押されたらシーンを切り替える
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}

	//カメラの更新
	camera->Update();

	for (Object3d *object3d : objects) {
		object3d->Update();
	}
}

void TitleScene::Draw() {
	//3Dオブジェトの描画準備
	Object3dCommon::GetInstance()->SetCommonDrawSettings();
	//3Dオブジェクトの描画
	for (Object3d *object3d : objects) {
		object3d->Draw();
	}

}
