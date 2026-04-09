#pragma once
#include "BaseScene.h"
#include <memory>
#include <string>


class AbstractSceneFactory {
public:
	virtual ~AbstractSceneFactory() = default;

	//シーン生成
	virtual std::unique_ptr<BaseScene> CreateScene(const std::string &sceneName) = 0;
};

