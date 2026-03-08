#pragma once
#include "BaseScene.h"
#include "AbstractSceneFactory.h"
#include <memory>
#include <string>

class SceneManager {
public:
	//シングストンインスタンスの取得
	static SceneManager *GetInstance();

	//ゲーム終了時の処理
	void Finalize();

	//ファクトリーのセット
	void SetSceneFactory(AbstractSceneFactory* sceneFactory){ sceneFactory_ = sceneFactory; }

	//シーンの切り替え予約
	void ChangeScene(const std::string &sceneName);

	//毎フレームの更新処理
	void Update();

	//描画処理
	void Draw();
private:
	SceneManager() = default;
	~SceneManager() = default;
	SceneManager(const SceneManager &) = delete;
	SceneManager operator=(const SceneManager &) = delete;

	//現在のシーン
	std::unique_ptr<BaseScene> currentScene_=nullptr;
	//次のシーン
	std::unique_ptr<BaseScene> nextScene_ = nullptr;

	//シーンファクトリー
	AbstractSceneFactory *sceneFactory_ = nullptr;
}; 

