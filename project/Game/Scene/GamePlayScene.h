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
#include "GameCameraManager.h"
#include "LevelManager.h"
#include "GamePlayUIManager.h"
#include "EnvironmentRenderer.h"
#include <memory>
#include <vector>
#include <list>
#include <string>
#include <filesystem>



class GamePlayScene :public BaseScene {
public:
	friend class GamePlayUIManager;
public:
	enum class Mode {
		Gameplay,
		Simulation
	};

	explicit GamePlayScene(Mode mode = Mode::Gameplay);

	// 初期化
	void Initialize() override;

	// 終了
	void Finalize() override;

	// 毎フレーム更新
	void Update() override;

	// 描画
	void Draw() override;

	// UIの更新

private:
	friend class SimulationManager;
	friend class MissilePresetManager;
	friend class LockOnManager;


	bool IsSimulationMode() const { return mode_ == Mode::Simulation; }
	void DrawOverlay();
	void SetDebugCameraActive(bool isActive);
	void ReloadSceneJson();
	void ResetEditorPreview();
	MissileTuning MakeMissileTuning(MissileType type) const;
	void SpawnEnemyFromSpawnPoint(size_t spawnPointIndex);
	bool IsEnemySpawnPointActive(size_t spawnPointIndex) const;
	void ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames);
	void TriggerEnemyReinforcements(const std::string &deadEnemyName);
	void UpdateEnemyRespawns();
	bool HasPendingEnemySpawns() const;
	void UpdateCinematicLockOnCamera();
	Mode mode_ = Mode::Gameplay;

	// シーンリソース
	std::unique_ptr<Camera> camera;
	std::unique_ptr<Sprite> sprite;
	std::unique_ptr<Object3d> groundModel;
	std::unique_ptr<Primitive> myShere;

	std::unique_ptr<Sprite> aimCursorSprite_;
	std::unique_ptr<Sprite> lockOnReticleSprite_;
	std::unique_ptr<Object3d> boundaryAlertObject_;
	std::unique_ptr<Object3d> ceilingBoundaryAlertObject_;


	// パーティクル

	// モデル
	std::vector<Object3d *> objects;

	// 音声データ
	SoundData soundData1;
	SoundData soundData2;

	// 再生中のボイスを管理するポインタ
	IXAudio2SourceVoice *pVoice1 = nullptr;
	IXAudio2SourceVoice *pVoice2 = nullptr;

	// プリミティブ
	std::unique_ptr<Primitive> myBox;

	std::unique_ptr<Object3d> myModelObject;

	// 表示切り替えフラグ

	// Partial Ring用パラメータ

	// Cylinder用パラメータ

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
	bool showSprite = false;
	std::unique_ptr<Model> skeletonLinesModel;
	std::unique_ptr<Object3d> skeletonLinesObject;

	// デバッグ用のコライダー描画
	std::unique_ptr<Object3d> debugColliderLinesObject;
	bool showDebugColliders = false;

	// =====================================================
	// デバッグ用フリーカメラ
	// フリーカメラ中は WASD: 移動, 矢印: 回転, Q/E: ロール
	// =====================================================
	std::unique_ptr<FlyCamera> debugFlyCamera_;
	bool isDebugCameraActive_ = false;
	bool isEditorPreviewPlaying_ = true;
	bool isCinematicLockOnCameraEnabled_ = false;
	bool isCinematicLockOnCameraInitialized_ = false;
	Vector3 cinematicLockOnCameraPosition_ = { 0.0f, 0.0f, 0.0f };
	Quaternion cinematicLockOnCameraRotation_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector3 cinematicLockOnCameraFocus_ = { 0.0f, 0.0f, 0.0f };
	Vector3 cinematicLockOnCameraBackDirection_ = { 0.0f, 0.0f, 1.0f };
	float cinematicLockOnCameraSideSign_ = 1.0f;
	float cinematicLockOnCameraSeparation_ = 0.0f;

	// UIと状態管理
	bool showModel = false;
	bool enableSkinning = true; // スキニング（ガワを動かす）の切り替え
	float modelScale = 1.0f;
	int currentAnimationIndex = 0;


	std::unique_ptr<Trail> missileTrail;        // 軌跡の計算を行うクラス
	std::unique_ptr<Object3d> trailObject;      // 軌跡を描画する実体

	float missileNormalSpeed = 1.5f; // 通常弾の速度
	float missileNormalScale = 0.3f;
	float missileNormalCollisionRadius = 0.3f;
	int missileNormalLifeTime = 120;
	float missileSpeed = 0.75f;   // ホーミングミサイルの速度
	float missileAmpX = 15.0f;   // X軸の旋回半径（横の広さ）
	float missileAmpZ = 15.0f;   // Z軸の旋回半径
	float missileAmpY = 3.0f;    // 上下に波打つ高さ
	float missileFreqY = 4.0f;    // 上下に波打つ細かさ（振動数）
	float missileBaseY = 5.0f;    // 基準となる飛行高度
	float missileHomingStrength = 0.085f;
	float missileHomingScale = 0.5f;
	float missileHomingCollisionRadius = 0.5f;
	float missileTrailWidth = 0.5f;
	int missileLifeTime = 240;
	float missileMuzzleOffset = 0.8f;

	std::unique_ptr<Player> player_;

	// 画面上に存在するミサイルを管理するマネージャ
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

	// 爆発エフェクト
	std::unique_ptr<ExplosionManager> explosionManager_;

	// ゲームオーバー演出用
	bool isGameOver_ = false;
	int gameOverTimer_ = 0;

	// シミュレーションツールUI用


	// =====================================================
	// マネージャークラス
	// =====================================================
		std::unique_ptr<SimulationManager> simulationManager_;
	std::unique_ptr<MissilePresetManager> missilePresetManager_;
	std::unique_ptr<LockOnManager> lockOnManager_;
std::unique_ptr<GameCameraManager> cameraManager_;
	std::unique_ptr<LevelManager> levelManager_;
	std::unique_ptr<EnvironmentRenderer> environmentRenderer_;
	std::unique_ptr<GamePlayUIManager> uiManager_;

	// JSONファイルが最後に更新された日時を記録する変数
	std::filesystem::file_time_type lastJsonWriteTime_;
	Enemy* aimAssistEnemy_ = nullptr;
	std::vector<Enemy*> multiLockTargets_;
	bool isMultiLockCharging_ = false;
	int multiLockChargeFrames_ = 0;
};






