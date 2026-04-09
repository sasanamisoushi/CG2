#include "GamePlayScene.h"
#include "ModelManager.h"
#include <Windows.h>
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "Input.h"
#include "ImGuiManager.h"
#include <externals/imgui/imgui.h>



void GamePlayScene::Initialize() {

	

	//カメラ・シーンリソース
	camera = std::make_unique<Camera>();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	//スプライトの初期化
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(SpriteCommon::GetInstance() , "resources/uvChecker.png");

	//モデル・パーティクル
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("multiMesh.obj");
	ModelManager::GetInstance()->CreateSphereModel("Sphere", 16);

	//1つ目のオブジェクト
	objA = std::make_unique<Object3d>();
	objA->Initialize(Object3dCommon::GetInstance());
	objA->SetModel("plane.obj");
	objA->transform.translate = { -2.0f,0.0f,0.0f };
	objects.push_back(objA.get());

	//２つ目のオブジェクト
	objB = std::make_unique<Object3d>();
	objB->Initialize(Object3dCommon::GetInstance());
	objB->SetModel("Sphere");
	objB->transform.translate = { 2.0f,0.0f,0.0f };
	objects.push_back(objB.get());

	//パーティクル
	particleManager = std::make_unique<ParticleManager>();
	particleManager->Initialize(DirectXCommon::GetInstance());
	particleManager->CreateParticleGroup("test", "resources/circle.png");
	particleEmitter = std::make_unique<ParticleEmitter>("test", Vector3{ 0.0f,0.0f,0.0f }, particleManager.get());

	//音声再生
	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");

	pVoice1=AudioManager::GetInstance()->PlayWave(soundData1, true);
	pVoice2=AudioManager::GetInstance()->PlayWave(soundData2, true);

}

void GamePlayScene::Finalize() {
	if (pVoice1) {
		pVoice1->Stop();
		pVoice2->DestroyVoice();
	}

	AudioManager::GetInstance()->UnloadWave(soundData1);
	AudioManager::GetInstance()->UnloadWave(soundData2);
}

void GamePlayScene::Update() {
	if (Input::GetInstance()->TriggerKey(DIK_0)) {
		OutputDebugStringA("HIt 0\n");
	}

	//カメラの更新
	camera->Update();

	for (Object3d *object3d : objects) {
		object3d->Update();
	}

	Vector2 size = sprite->GetSize();
	size.x = 300.0f;
	size.y = 300.0f;
	sprite->SetSize(size);

	particleEmitter->Update();
	particleManager->Update(camera.get());

#ifdef ENABLE_IMGUI
	//開発用UIの処理
	ImGui::ShowDemoWindow();

	//ウィンドウのサイズを設定
	ImGui::SetNextWindowSize(ImVec2(500.0f, 100.0f));

	//ウィンドウの作成
	ImGui::Begin("Exercise");

	//スプライトの座標を配列に格納
	Vector2 currentPos = sprite->GetPosition();
	float pos[2] = { currentPos.x,currentPos.y };

	//スライダーでスプライトの座標を変更できるようにする
	ImGui::SliderFloat2("Sprite Position", pos, 0.0f, 1280.0f, "%.1f");

	//スライダーで変更された座標をスプライトに反映
	sprite->SetPosition({ pos[0],pos[1] });

	ImGui::End();

	ImGui::Begin("Settings");

	
	if (objB!= nullptr) {
		// ライトのデータ（ポインタ）を取得
		DirectionalLight *lightData = objB->GetDirectionalLightData();

		if (lightData != nullptr) {
			// ① ライトの色（RGBA）
			ImGui::ColorEdit4("Light Color", &lightData->color.x);

			// ② ライトの向き（X, Y, Z）
			ImGui::DragFloat3("Light Direction", &lightData->direction.x, 0.01f, -1.0f, 1.0f);

			// ③ ライトの明るさ
			ImGui::DragFloat("Light Intensity", &lightData->intensity, 0.01f, 0.0f, 10.0f);
		}

		// ① 現在のスケールを取得する（関数名はご自身の環境に合わせてください）
		Vector3 currentScale = objB->GetScale();

		// ImGuiで使いやすいように配列に入れる
		float scale[3] = { currentScale.x, currentScale.y, currentScale.z };

		// ② スケールを変更できるスライダー（X, Y, Z を個別にイジれる）
		ImGui::DragFloat3("Scale", scale, 0.01f, 0.1f, 10.0f);

		// ③ 変更されたスケールをオブジェクトに反映する
		objB->SetScale({ scale[0], scale[1], scale[2] });
	}

	ImGui::End();
#endif
	sprite->Update();
}

void GamePlayScene::Draw() {
	//3Dオブジェトの描画準備
	Object3dCommon::GetInstance()->SetCommonDrawSettings();
	//3Dオブジェクトの描画
	for (Object3d *object3d : objects) {
		object3d->Draw();
	}

	Model *sphereModel = ModelManager::GetInstance()->FindModel("Sphere");
	if (sphereModel) {
		sphereModel->Draw();
	}

	//Spriteの描画基準
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	//スプライト描画
	sprite->Draw();

	particleManager->Draw();
}
