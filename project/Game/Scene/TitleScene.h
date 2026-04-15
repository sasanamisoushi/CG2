#pragma once
#include "engine/Camera/Camera.h"
#include "3D/Object3d.h"
#include "Game/base/BaseScene.h"

class TitleScene : public BaseScene {
public:
	void Initialize() override;

	void Finalize() override;

	void Update() override;

	void Draw() override;
private:
	std::unique_ptr<Camera> camera;

	//モデル
	std::vector<Object3d *> objects;
	std::unique_ptr<Object3d> objA;

};

