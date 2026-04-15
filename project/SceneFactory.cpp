#include "SceneFactory.h"
#include "Game/Scene/TitleScene.h"
#include "Game/Scene/GamePlayScene.h"
#include <cassert>

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string &sceneName) {
	std::unique_ptr<BaseScene> newScene = nullptr;

	//文字列に応じて、生成するシーンを変える
	if (sceneName == "TITLE") {
		newScene = std::make_unique<TitleScene>();
	} else if (sceneName == "GAMEPLAY") {
		newScene = std::make_unique<GamePlayScene>();
	}

	//該当するシーン名がない場合エラーで止める
	assert(newScene != nullptr && "Unknown scene name!");

	return newScene;
}