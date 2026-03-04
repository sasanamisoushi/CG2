#pragma once
#include "Camera.h"
#include "Sprite.h"
#include "Object3d.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "AudioManager.h"
#include <memory>
#include <vector>
#include "Framework.h"

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

	//シーンリソース
	std::unique_ptr<Camera> camera;
	std::unique_ptr<Sprite> sprite;

	//パーティクル
	std::unique_ptr<ParticleManager> particleManager;
	std::unique_ptr<ParticleEmitter> particleEmitter;

	//モデル
	std::vector<Object3d *> objects;
	std::unique_ptr<Object3d> objA;
	std::unique_ptr<Object3d> objB;

	//音声データ
	SoundData soundData1;

};

