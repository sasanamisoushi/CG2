#pragma once
#include "engine/Camera/Camera.h"
#include "2D/Sprite.h"
#include "Game/base/BaseScene.h"
#include <memory>

class GameOverScene : public BaseScene {
public:
	void Initialize() override;

	void Finalize() override;

	void Update() override;

	void Draw() override;
private:
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<Sprite> gameOverSprite_;
};
