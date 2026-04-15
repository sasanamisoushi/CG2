#pragma once
#include "engine/Camera/Camera.h"
#include "2D/Sprite.h"
#include "3D/Object3d.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/Particle/ParticleEmitter.h"
#include "engine/Audio/AudioManager.h"
#include "Game/base/BaseScene.h"
#include "3D/Skybox.h"
#include <memory>
#include <vector>



class GamePlayScene :public BaseScene {
public:

	//初期化
	void Initialize() override;

	//終了
	void Finalize() override;

	//毎フレーム更新
	void Update() override;

	//描画
	void Draw() override;
private:

	//シーンリソース
	std::unique_ptr<Camera> camera;
	std::unique_ptr<Sprite> sprite;

	std::unique_ptr<Skybox> skybox;

	//パーティクル
	std::unique_ptr<ParticleManager> particleManager;
	std::unique_ptr<ParticleEmitter> particleEmitter;

	//モデル
	std::vector<Object3d *> objects;
	std::unique_ptr<Object3d> objA;
	std::unique_ptr<Object3d> objB;

	//音声データ
	SoundData soundData1;
	SoundData soundData2;

	// 再生中のボイスを管理するポイインタ
	IXAudio2SourceVoice *pVoice1 = nullptr;
	IXAudio2SourceVoice *pVoice2 = nullptr;
};

