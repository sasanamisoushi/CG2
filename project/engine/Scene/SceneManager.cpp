#include "SceneManager.h"
#include <cassert>

SceneManager *SceneManager::GetInstance() {
	static SceneManager instance;
	return &instance;
}

void SceneManager::Finalize() {
	if (currentScene_) {
		currentScene_->Finalize();
		currentScene_.reset();
	}
}

void SceneManager::ChangeScene(const std::string &sceneName) {
	// ファクトリーがセットされていなければエラー
	assert(sceneFactory_ != nullptr && "SceneFactory is not set!");

	nextScene_ = sceneFactory_->CreateScene(sceneName);
}

void SceneManager::Update() {
	//次のシーンを予約
	if (nextScene_) {
		//現在のシーンがあれば終了処理を呼ぶ
		if (currentScene_) {
			currentScene_->Finalize();
		}

		//シーンを移行する
		currentScene_ = std::move(nextScene_);

		//新しいシーンの初期化を呼ぶ
		currentScene_->Initialize();
	}

	//現在のシーンの更新処理
	if (currentScene_) {
		currentScene_->Update();
	}
}

void SceneManager::Draw() {
	//現在のシーンの描画処理
	if (currentScene_) {
		currentScene_->Draw();
	}
}
