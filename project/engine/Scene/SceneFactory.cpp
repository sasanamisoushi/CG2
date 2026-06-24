#include "SceneFactory.h"
#include "Game/Scene/TitleScene.h"
#include "Game/Scene/GamePlayScene.h"
#include "Game/Scene/GameOverScene.h"
#include "Game/Scene/ClearScene.h"
#include <cassert>

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string &sceneName) {
	std::unique_ptr<BaseScene> newScene = nullptr;

	//譁・ｭ怜・縺ｫ蠢懊§縺ｦ縲∫函謌舌☆繧九す繝ｼ繝ｳ繧貞､峨∴繧・
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

	//隧ｲ蠖薙☆繧九す繝ｼ繝ｳ蜷阪′縺ｪ縺・ｴ蜷医お繝ｩ繝ｼ縺ｧ豁｢繧√ｋ
	assert(newScene != nullptr && "Unknown scene name!");

	return newScene;
}


