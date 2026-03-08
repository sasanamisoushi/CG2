#pragma once
#include <memory>
#include "Framework.h"
#include "SceneFactory.h"

class Game : public Framework {
public:

	//初期化
	void Initialize() override;

	//終了
	void Finalize() override;

	//更新
	void Update() override;

	//描画
	void Draw() override;

private:

	//シーンファクトリー
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
};

