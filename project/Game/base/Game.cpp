#include "Game.h"
#include "ModelManager.h"
#include "AudioManager.h"
#include "SrvManager.h"
#include "SceneManager.h"

void Game::Initialize() {

	Framework::Initialize();

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
	DirectXCommon::GetInstance()->PreDraw(SrvManager::GetInstance());

	// シーンの描画
	SceneManager::GetInstance()->Draw();

#ifdef  ENABLE_IMGUI
	//-------ImGuiの描画-------
	imGuiManager->EndFrame(DirectXCommon::GetInstance()->GetCommandList());
#endif
	//描画後処理
	DirectXCommon::GetInstance()->PostDraw();

}