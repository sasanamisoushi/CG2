#pragma once
#include "AbstractSceneFactory.h"

class SceneFactory :public AbstractSceneFactory {
public:
	//シーンの生成
	std::unique_ptr<BaseScene> CreateScene(const std::string &sceneName)override;
};

