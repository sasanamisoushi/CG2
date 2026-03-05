#include "Game.h"

void Game::Initialize() {

	Framework::Initialize();


	//---------カメラ・シーンリソース---------
	//カメラの初期化
	camera = std::make_unique<Camera>();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	object3dCommon->SetDefaultCamera(camera.get());

	//スプライトの初期化
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon.get(), "resources/uvChecker.png");

	//---------モデル・パーティクル----------
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("multiMesh.obj");

	// 1つ目のオブジェクト（plane）
	objA = std::make_unique<Object3d>();
	objA->Initialize(object3dCommon.get());
	//初期化済みの3Dオブジェトにモデルを紐づける
	objA->SetModel("plane.obj");
	objA->transform.translate = { -2.0f, 0.0f, 0.0f }; // 左に配置
	objects.push_back(objA.get());

	// 2つ目のオブジェクト
	objB = std::make_unique<Object3d>();
	objB->Initialize(object3dCommon.get());
	//初期化済みの3Dオブジェトにモデルを紐づける
	objB->SetModel("plane.obj");
	objB->transform.translate = { 2.0f, 0.0f, 0.0f }; // 右に配置
	objects.push_back(objB.get());

	//---------パーティクル----------
	//Particleマネージャーの初期化
	particleManager = std::make_unique<ParticleManager>();
	particleManager->Initialize(dxCommon.get());
	particleManager->CreateParticleGroup("test", "resources/circle.png");

	particleEmitter = std::make_unique<ParticleEmitter>("test", Vector3{ 0.0f, 0.0f, 0.0f }, particleManager.get());

	//---------音声再生----------
	//音声読み込み
	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");

	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");

	//音声再生
	AudioManager::GetInstance()->PlayWave(soundData1, true);
	AudioManager::GetInstance()->PlayWave(soundData2, true);
}

void Game::Finalize() {

	Framework::Finalize();

	AudioManager::GetInstance()->UnloadWave(soundData1);

	AudioManager::GetInstance()->UnloadWave(soundData2);

	//３Dモデルマネージャの初期化
	ModelManager::GetInstance()->Finalize();

	SrvManager::GetInstance()->Finalize();

	//WindowaAPIの終了処理
	winApp->Finalize();
}

void Game::Update() {

	Framework::Update();

	//-----ImGuiのフレーム開始処理-----
	imGuiManager->BeginFrame();

	if (input->TriggerKey(DIK_0)) {
		OutputDebugStringA("Hit 0\n");
	}

	//カメラの更新
	camera->Update();

	for (Object3d *object3d : objects) {
		object3d->Update();
	}

	//現在の座標を変数で受ける
	Vector2 position = sprite->GetPosition();
	//座標を変更する
	position = Vector2{ 100.0f,100.0f };
	//変更を反映する
	sprite->SetPosition(position);

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
#endif
	sprite->Update();
}

void Game::Draw() {

	//描画前処理
	dxCommon->PreDraw(SrvManager::GetInstance());


	//3Dオブジェトの描画準備
	object3dCommon->SetCommonDrawSettings();
	//3Dオブジェクトの描画
	for (Object3d *object3d : objects) {
		object3d->Draw();
	}
	


	//Spriteの描画基準
	spriteCommon->SetCommonPipelineState();
	//スプライト描画
	sprite->Draw();
	

	particleManager->Draw();

	//-------ImGuiの描画-------
	imGuiManager->EndFrame(dxCommon->GetCommandList());

	//描画後処理
	dxCommon->PostDraw();

}