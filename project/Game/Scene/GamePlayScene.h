#pragma once
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "2D/Sprite.h"
#include "3D/Object3d.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/Particle/ParticleEmitter.h"
#include "engine/Particle/ExplosionManager.h"
#include "engine/Audio/AudioManager.h"
#include "engine/Utility/StageLoader.h"
#include "Game/base/BaseScene.h"
#include "Game/Player/Player.h"
#include "3D/Skybox.h"
#include "3D/primitive.h"
#include "3D/Animation.h"
#include "3D/Model.h"
#include "3D/Trail.h"
#include "Game/bullet/MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/enemy/EnemyEventManager.h"
#include "Game/obstacle/Obstacle.h"
#include <memory>
#include <vector>
#include <list>
#include <string>
#include <filesystem>



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

	// UIの更新
	void UpdateUI();

private:
	void DrawOverlay();
	void SetDebugCameraActive(bool isActive);
	void ReloadSceneJson();
	void ResetEditorPreview();
	void SpawnEnemiesFromSpawnPoints();
	void SpawnEnemyFromSpawnPoint(size_t spawnPointIndex);
	void ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames);
	void TriggerEnemyReinforcements(const std::string &deadEnemyName);
	void UpdateEnemyRespawns();
	bool IsEnemySpawnPointActive(size_t spawnPointIndex) const;
	void UpdateLockOn(Camera *activeCamera, bool shouldUpdateGame);
	Enemy *FindLockOnTarget(Camera *activeCamera) const;
	bool IsLockedEnemyAlive() const;
	void UpdateGameplayCamera();
	void UpdateCinematicLockOnCamera();

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

	std::unique_ptr<Object3d> myModelObject;

	// 表示切替フラグ
	bool showNormalRing = false;
	bool showPartialRing = false;
	bool showCylinder = false;

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

	// アニメーション用
	Animation animationData;
	float animationTime = 0.0f;
	bool playAnimation = true;

	// スケルトン
	Skeleton skeleton;
	bool showBones = false;
	bool showPlane = false;
	bool showSphere = false;
	bool showBox = false;
	bool showTrail = false;
	bool showSkybox = false;
	bool showSprite = false;
	std::unique_ptr<Model> skeletonLinesModel;
	std::unique_ptr<Object3d> skeletonLinesObject;

	// デバッグ用のコライダー描画
	std::unique_ptr<Object3d> debugColliderLinesObject;
	bool showDebugColliders = true;

	// =====================================================
	// デバッグ用フリーカメラ
	// フリーカメラ中は WASD: 移動, 矢印: 回転, Q/E: ロール
	// =====================================================
	std::unique_ptr<FlyCamera> debugFlyCamera_;
	bool isDebugCameraActive_ = false;
	bool isEditorPreviewPlaying_ = true;
	bool isCinematicLockOnCameraEnabled_ = true;
	bool isCinematicLockOnCameraInitialized_ = false;
	Vector3 cinematicLockOnCameraPosition_ = { 0.0f, 0.0f, 0.0f };
	Quaternion cinematicLockOnCameraRotation_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector3 cinematicLockOnCameraFocus_ = { 0.0f, 0.0f, 0.0f };
	Vector3 cinematicLockOnCameraBackDirection_ = { 0.0f, 0.0f, 1.0f };
	float cinematicLockOnCameraSideSign_ = 1.0f;
	float cinematicLockOnCameraSeparation_ = 0.0f;

	// UIと状態管理
	bool showParticles = false;
	bool showModel = false;
	bool enableSkinning = false; // スキニング（ガワを動かす）の切り替え
	float modelScale = 0.01f;
	int currentAnimationIndex = 0;


	std::unique_ptr<Trail> missileTrail;        // 軌跡の計算を行うクラス
	std::unique_ptr<Object3d> trailObject;      // 軌跡を描画する実体

	float missileSpeed = 0.05f;   // 飛ぶスピード
	float missileAmpX = 15.0f;   // X軸の旋回半径（振り幅）
	float missileAmpZ = 15.0f;   // Z軸の旋回半径
	float missileAmpY = 3.0f;    // 上下に波打つ高さ
	float missileFreqY = 4.0f;    // 上下に波打つ細かさ（周波数）
	float missileBaseY = 5.0f;    // 基準となる飛行高度

	std::unique_ptr<Player> player_;

	// 画面上に存在するすべてのミサイルを管理するリスト
	std::unique_ptr<MissileManager> missileManager_;


	// 敵
	std::list<std::unique_ptr<Enemy>> enemies_;
	std::unique_ptr<EnemyBulletManager> enemyBulletManager_;
	std::vector<EnemySpawnData> enemySpawns_;
	std::vector<int> enemyRespawnTimers_;
	EnemyEventManager enemyEventManager_;
	Enemy *lockedEnemy_ = nullptr;


	// 障害物
	std::list<std::unique_ptr<Obstacle>> obstacles_;

	// ImGuiで敵を出すための座標変数
	float newEnemyPos[3] = { 0.0f, 0.0f, 50.0f };

	// 爆破エフェクト
	std::unique_ptr<ExplosionManager> explosionManager_;

	// ゲームオーバー演出用
	bool isGameOver_ = false;
	int gameOverTimer_ = 0;


	// JSONファイルが最後に更新された日時を記録する変数
	std::filesystem::file_time_type lastJsonWriteTime_;
};
