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
	//-----ImGuiのフレーム開始処理-----
	imGuiManager->BeginFrame();

	postEffect_->DrawImGui();
#endif

	// ポストエフェクトの更新（自動再生などを計算）
	postEffect_->Update();

	SceneManager::GetInstance()->Update();
}

void Game::Draw() {

	//描画前処理
	DirectXCommon::GetInstance()->PreDraw(SrvManager::GetInstance(), renderTexture_->GetRtvHandle());

	// シーンの描画
	SceneManager::GetInstance()->Draw();

	// 描画先を本来の画面に戻し、背景を黒などでクリアする
	DirectXCommon::GetInstance()->PreDrawSwapchain();

	// 深度バッファを「読み込みモード(SRV)」に切り替え
	DirectXCommon::GetInstance()->SetDepthStateToSRV();

	// TextureManager等を使って、ノイズ画像（今回はuvChecker）のGPUハンドルを第3引数に渡す！
	auto noise0Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise0.png");
	auto noise1Handle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/noise1.png");

	// 録画したRenderTextureを全画面に貼り付ける！
	postEffect_->Draw(renderTexture_->GetSrvHandle(),
		SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_),
		noise0Handle,
		noise1Handle);

	// 深度バッファを元の「書き込みモード(DSV)」に戻す
	DirectXCommon::GetInstance()->SetDepthStateToDSV();


#ifdef  ENABLE_IMGUI
	//-------ImGuiの描画-------
	imGuiManager->EndFrame(DirectXCommon::GetInstance()->GetCommandList());
#endif
	//描画後処理
	DirectXCommon::GetInstance()->PostDraw();

}