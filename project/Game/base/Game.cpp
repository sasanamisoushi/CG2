#include "Game.h"
#include "3D/ModelManager.h"
#include "engine/Audio/AudioManager.h"
#include "engine/Graphics/SrvManager.h"
#include "SceneManager.h"

void Game::Initialize() {

	Framework::Initialize();

	// RenderTextureの生成と初期化
	renderTexture_ = std::make_unique<RenderTexture>();
	renderTexture_->Initialize(WinApp::kClientWidth, WinApp::kClientHeight);

	postEffect_ = std::make_unique<PostEffect>();
	postEffect_->Initialize();

	//シーンファクトリーを生成
	sceneFactory_ = std::make_unique<SceneFactory>();

	//マネージャーにファクトリーをセット
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

	//最初のシーンをセット
	SceneManager::GetInstance()->ChangeScene("TITLE");
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
#endif

	SceneManager::GetInstance()->Update();
}

void Game::Draw() {

	//描画前処理
	DirectXCommon::GetInstance()->PreDraw(SrvManager::GetInstance(), renderTexture_->GetRtvHandle());

	// シーンの描画
	SceneManager::GetInstance()->Draw();

	// 描画先を本来の画面に戻し、背景を黒などでクリアする
	DirectXCommon::GetInstance()->PreDrawSwapchain();

	// 録画したRenderTextureを全画面に貼り付ける！
	postEffect_->Draw(renderTexture_->GetSrvHandle());


#ifdef  ENABLE_IMGUI
	//-------ImGuiの描画-------
	imGuiManager->EndFrame(DirectXCommon::GetInstance()->GetCommandList());
#endif
	//描画後処理
	DirectXCommon::GetInstance()->PostDraw();

}