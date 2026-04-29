#pragma once
#include "engine/Camera/Camera.h"
#include "2D/Sprite.h"
#include "3D/Object3d.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/Particle/ParticleEmitter.h"
#include "engine/Audio/AudioManager.h"
#include "Game/base/BaseScene.h"
#include "3D/Skybox.h"
#include "3D/primitive.h"
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

	//音声データ
	SoundData soundData1;
	SoundData soundData2;

	// 再生中のボイスを管理するポイインタ
	IXAudio2SourceVoice *pVoice1 = nullptr;
	IXAudio2SourceVoice *pVoice2 = nullptr;

	// プリミティブ
	std::unique_ptr<Primitive> myPlane;
	std::unique_ptr<Primitive> myShere;
	std::unique_ptr<Primitive> myBox;
	std::unique_ptr<Primitive> myRing;
	std::unique_ptr<Primitive> myPartialRing;
	std::unique_ptr<Primitive> myCylinder;

	// 表示切替フラグ
	bool showNormalRing = true;
	bool showPartialRing = true;
	bool showCylinder = true;

	// Partial Ring用パラメータ
	int prSubdivision = 64;
	float prOuterRadius = 1.2f;
	float prInnerRadius = 0.4f;
	bool prIsUvHorizontal = false;
	float prInnerColor[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
	float prOuterColor[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
	float prStartAngle = 0.0f;
	float prEndAngle = 180.0f;
	float prFadeAngle = 30.0f;

	// Cylinder用パラメータ
	float cylinderPos[3] = { 0.0f, 0.0f, 0.0f };
	float cylinderScale[3] = { 1.0f, 1.0f, 1.0f };
	float cylinderUVOffset[2] = { 0.0f, 0.0f };
	float cylinderUVScrollSpeed[2] = { 0.01f, 0.0f };
	float cylinderAlphaReference = 0.0f;

	int cylinderSubdivision = 32;
	int cylinderVerticalSubdivision = 1;
	float cylinderTopRadiusX = 1.0f;
	float cylinderTopRadiusZ = 1.0f;
	float cylinderBottomRadiusX = 1.0f;
	float cylinderBottomRadiusZ = 1.0f;
	float cylinderHeight = 3.0f;
	float cylinderTopColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float cylinderBottomColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float cylinderStartAngle = 0.0f;
	float cylinderEndAngle = 360.0f;
	bool cylinderIsUvFlipped = false;
};

