#include "SceneFactory.h"
#include "Game/Scene/TitleScene.h"
#include "Game/Scene/GamePlayScene.h"
#include "Game/Scene/GameOverScene.h"
#include "Game/Scene/ClearScene.h"
#include <cassert>

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string &sceneName) {
	std::unique_ptr<BaseScene> newScene = nullptr;

	// 文字列に応じて、生成するシーンを変える
	if (sceneName == "TITLE") {
		newScene = std::make_unique<TitleScene>();
	} else if (sceneName == "GAMEPLAY") {
		newScene = std::make_unique<GamePlayScene>();
	} else if (sceneName == "SIMULATION") {
		newScene = std::make_unique<GamePlayScene>(GamePlayScene::Mode::Simulation);
	} else if (sceneName == "GAMEOVER") {
		newScene = std::make_unique<GameOverScene>();
	} else if (sceneName == "CLEAR") {
		newScene = std::make_unique<ClearScene>();
	}

	// 該当するシーン名がない場合はエラーで止める
	assert(newScene != nullptr && "Unknown scene name!");

	return newScene;
}


