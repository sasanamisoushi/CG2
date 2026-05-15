#include "Game.h"
#include "3D/ModelManager.h"
#include "engine/Audio/AudioManager.h"
#include "engine/Graphics/SrvManager.h"
#include "SceneManager.h"
#include <engine/Resource/TextureManager.h>

void Game::Initialize() {

	Framework::Initialize();

	// RenderTextureの生成と初期化
	renderTexture_ = std::make_unique<RenderTexture>();
	renderTexture_->Initialize(WinApp::kClientWidth, WinApp::kClientHeight);

	// ポストエフェクト用テクスチャも初期化
	postEffectTexture_ = std::make_unique<RenderTexture>();
	postEffectTexture_->Initialize(WinApp::kClientWidth, WinApp::kClientHeight);


	postEffect_ = std::make_unique<PostEffect>();
	postEffect_->Initialize();

	// 深度バッファのSRVを作成する！
	depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
	DirectXCommon::GetInstance()->CreateDepthSrv(SrvManager::GetInstance()->GetCPUDescriptorHandle(depthSrvIndex_));

	//シーンファクトリーを生成
	sceneFactory_ = std::make_unique<SceneFactory>();

	//マネージャーにファクトリーをセット
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

	//最初のシーンをセット
	SceneManager::GetInstance()->ChangeScene("TITLE");

	// ノイズ画像を2種類とも読み込んでおく
	TextureManager::GetInstance()->LoadTexture("resources/noise0.png");
	TextureManager::GetInstance()->LoadTexture("resources/noise1.png");
}

void Game::Finalize() {

	

	SceneManager::GetInstance()->Finalize();

	//３Dモデルマネージャ・SRVマネージャの終了処理
	ModelManager::GetInstance()->Finalize();
	SrvManager::GetInstance()->Finalize();

	Framework::Finalize();
	winApp->Finalize();
	
}

void Game::Update() {

	Framework::Update();

#ifdef ENABLE_IMGUI
	// F1キーでImGuiの表示・非表示を切り替え
	if (Input::GetInstance()->TriggerKey(DIK_F1)) {
		showImGui_ = !showImGui_;
		ImGuiManager::SetVisible(showImGui_);
	}

	//-----ImGuiのフレーム開始処理-----
	if (showImGui_) {
		imGuiManager->BeginFrame();

		// 画面全体をImGuiの「ドッキングエリア」にする
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		postEffect_->DrawImGui();
	}
#endif

	// ポストエフェクトの更新（自動再生などを計算）
	postEffect_->Update();

	SceneManager::GetInstance()->Update();
}

void Game::Draw() {

	// ==========================================
	// 1. オフスクリーン描画（3Dシーンを 1枚目「renderTexture_」に録画）
	// ==========================================
	DirectXCommon::GetInstance()->PreDraw(SrvManager::GetInstance(), renderTexture_->GetRtvHandle());
	SceneManager::GetInstance()->Draw();

	// ==========================================
	// 2. ポストエフェクト描画
	// ==========================================
#ifdef ENABLE_IMGUI
	if (showImGui_) {
		// ImGuiあり：ポストエフェクトを 2枚目「postEffectTexture_」に描画する
		DirectXCommon::GetInstance()->PreDraw(SrvManager::GetInstance(), postEffectTexture_->GetRtvHandle());

		// 深度バッファをSRV（読み込み）に切り替え
		DirectXCommon::GetInstance()->SetDepthStateToSRV();

		auto noise0Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise0.png");
		auto noise1Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise1.png");

		// 1枚目(renderTexture_)を入力にして、2枚目にポストエフェクトをかける！
		postEffect_->Draw(
			renderTexture_->GetSrvHandle(),
			SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_),
			noise0Handle,
			noise1Handle
		);

		// 深度バッファをDSV（書き込み）に戻す
		DirectXCommon::GetInstance()->SetDepthStateToDSV();

		// ==========================================
		// 3. 実際の画面（スワップチェーン）への描画 (ImGui経由)
		// ==========================================
		DirectXCommon::GetInstance()->PreDrawSwapchain();

		// ゲーム画面をImGuiの「一つのウィンドウ」として表示する！
		ImGui::Begin("Game View");

		// ウィンドウのサイズを取得し、そのサイズに合わせて2枚目のテクスチャを画像として貼り付ける
		ImVec2 windowSize = ImGui::GetContentRegionAvail();
		ImGui::Image((ImTextureID)postEffectTexture_->GetSrvHandle().ptr, windowSize);

		ImGui::End();

		// ImGuiの描画コマンドを確定して、コマンドリストに積む
		imGuiManager->EndFrame(DirectXCommon::GetInstance()->GetCommandList());
	} else 
#endif
	{
		// ImGuiなし（または非表示）：ポストエフェクトを直接スワップチェーンに描画する
		DirectXCommon::GetInstance()->PreDrawSwapchain();

		// 深度バッファをSRV（読み込み）に切り替え
		DirectXCommon::GetInstance()->SetDepthStateToSRV();

		auto noise0Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise0.png");
		auto noise1Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise1.png");

		// 1枚目(renderTexture_)を入力にして、スワップチェーンに直接描画！
		postEffect_->Draw(
			renderTexture_->GetSrvHandle(),
			SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_),
			noise0Handle,
			noise1Handle
		);

		// 深度バッファをDSV（書き込み）に戻す
		DirectXCommon::GetInstance()->SetDepthStateToDSV();
	}

	//描画後処理
	DirectXCommon::GetInstance()->PostDraw();

}