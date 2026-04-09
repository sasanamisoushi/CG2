#pragma once
#include "Camera.h"
#include "Object3d.h"
#include "BaseScene.h"

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

