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
	enum class Mode {
		Gameplay,
		Simulation
	};

	explicit GamePlayScene(Mode mode = Mode::Gameplay);

	//蛻晄悄蛹・
	void Initialize() override;

	//邨ゆｺ・
	void Finalize() override;

	//豈弱ヵ繝ｬ繝ｼ繝譖ｴ譁ｰ
	void Update() override;

	//謠冗判
	void Draw() override;

	// UI縺ｮ譖ｴ譁ｰ
	void UpdateUI();

private:
	bool IsSimulationMode() const { return mode_ == Mode::Simulation; }
	void DrawOverlay();
	void DrawSimulationScreenUI();
	void DrawSimulationSaveControls();
	void DrawGameplayActionControls();
	void DrawMissileSettingsUI();
	void SetDebugCameraActive(bool isActive);
	void ReloadSceneJson();
	void ResetEditorPreview();
	MissileTuning MakeMissileTuning(MissileType type) const;
	void FirePlayerMissile(MissileType type, Enemy *target = nullptr, float horizontalOffset = 0.0f);
	bool SaveCurrentSimulationLayoutToSceneJson(const std::string &filePath);
	bool SaveNamedSimulationAction(const std::string &filePath, const std::string &actionName);
	bool ApplySimulationAction(const std::string &filePath, const std::string &actionName);
	void RefreshSimulationActionNames();
	bool SaveMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName);
	bool ApplyMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName);
	void RefreshMissilePresetNames();
	void SpawnEnemyFromSpawnPoint(size_t spawnPointIndex);
	bool IsEnemySpawnPointActive(size_t spawnPointIndex) const;
	void UpdateCinematicLockOnCamera();
	Mode mode_ = Mode::Gameplay;

	//繧ｷ繝ｼ繝ｳ繝ｪ繧ｽ繝ｼ繧ｹ
	std::unique_ptr<Camera> camera;
	std::unique_ptr<Sprite> sprite;
	std::unique_ptr<Object3d> groundModel;
	std::unique_ptr<Primitive> myShere;
	std::unique_ptr<Skybox> skybox;

	std::unique_ptr<Sprite> aimCursorSprite_;
	std::unique_ptr<Sprite> lockOnReticleSprite_;
	std::unique_ptr<Object3d> boundaryAlertObject_;
	std::unique_ptr<Object3d> ceilingBoundaryAlertObject_;


	//繝代・繝・ぅ繧ｯ繝ｫ
	std::unique_ptr<ParticleManager> particleManager;
	std::unique_ptr<ParticleEmitter> particleEmitter;

	//繝｢繝・Ν
	std::vector<Object3d *> objects;

	//髻ｳ螢ｰ繝・・繧ｿ
	SoundData soundData1;
	SoundData soundData2;

	// 蜀咲函荳ｭ縺ｮ繝懊う繧ｹ繧堤ｮ｡逅・☆繧九・繧､繧､繝ｳ繧ｿ
	IXAudio2SourceVoice *pVoice1 = nullptr;
	IXAudio2SourceVoice *pVoice2 = nullptr;

	// 繝励Μ繝溘ユ繧｣繝・
	std::unique_ptr<Primitive> boundaryAlertPlane_;
	std::unique_ptr<Primitive> myBox;
	std::unique_ptr<Primitive> myRing;
	std::unique_ptr<Primitive> myPartialRing;
	std::unique_ptr<Primitive> myCylinder;

	std::unique_ptr<Object3d> myModelObject;

	// 陦ｨ遉ｺ蛻・崛繝輔Λ繧ｰ
	bool showNormalRing = false;
	bool showPartialRing = false;
	bool showCylinder = false;

	// Partial Ring逕ｨ繝代Λ繝｡繝ｼ繧ｿ
	int prSubdivision = 64;
	float prOuterRadius = 1.2f;
	float prInnerRadius = 0.4f;
	bool prIsUvHorizontal = false;
	float prInnerColor[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
	float prOuterColor[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
	float prStartAngle = 0.0f;
	float prEndAngle = 180.0f;
	float prFadeAngle = 30.0f;

	// Cylinder逕ｨ繝代Λ繝｡繝ｼ繧ｿ
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

	// 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ逕ｨ
	Animation animationData;
	float animationTime = 0.0f;
	bool playAnimation = true;

	// 繧ｹ繧ｱ繝ｫ繝医Φ
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

	// 繝・ヰ繝・げ逕ｨ縺ｮ繧ｳ繝ｩ繧､繝繝ｼ謠冗判
	std::unique_ptr<Object3d> debugColliderLinesObject;
	bool showDebugColliders = true;

	// =====================================================
	// 繝・ヰ繝・げ逕ｨ繝輔Μ繝ｼ繧ｫ繝｡繝ｩ
	// 繝輔Μ繝ｼ繧ｫ繝｡繝ｩ荳ｭ縺ｯ WASD: 遘ｻ蜍・ 遏｢蜊ｰ: 蝗櫁ｻ｢, Q/E: 繝ｭ繝ｼ繝ｫ
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

	// UI縺ｨ迥ｶ諷狗ｮ｡逅・
	bool showParticles = false;
	bool showModel = true;
	bool enableSkinning = true; // 繧ｹ繧ｭ繝九Φ繧ｰ・医ぎ繝ｯ繧貞虚縺九☆・峨・蛻・ｊ譖ｿ縺・
	float modelScale = 1.0f;
	int currentAnimationIndex = 0;


	std::unique_ptr<Trail> missileTrail;        // 霆瑚ｷ｡縺ｮ險育ｮ励ｒ陦後≧繧ｯ繝ｩ繧ｹ
	std::unique_ptr<Object3d> trailObject;      // 霆瑚ｷ｡繧呈緒逕ｻ縺吶ｋ螳滉ｽ・

	float missileNormalSpeed = 1.5f; // 騾壼ｸｸ蠑ｾ縺ｮ騾溷ｺｦ
	float missileNormalScale = 0.3f;
	float missileNormalCollisionRadius = 0.3f;
	int missileNormalLifeTime = 120;
	float missileSpeed = 0.75f;   // 繝帙・繝溘Φ繧ｰ繝溘し繧､繝ｫ縺ｮ騾溷ｺｦ
	float missileAmpX = 15.0f;   // X霆ｸ縺ｮ譌句屓蜊雁ｾ・ｼ域険繧雁ｹ・ｼ・
	float missileAmpZ = 15.0f;   // Z霆ｸ縺ｮ譌句屓蜊雁ｾ・
	float missileAmpY = 3.0f;    // 荳贋ｸ九↓豕｢謇薙▽鬮倥＆
	float missileFreqY = 4.0f;    // 荳贋ｸ九↓豕｢謇薙▽邏ｰ縺九＆・亥捉豕｢謨ｰ・・
	float missileBaseY = 5.0f;    // 蝓ｺ貅悶→縺ｪ繧矩｣幄｡碁ｫ伜ｺｦ
	float missileHomingStrength = 0.085f;
	float missileHomingScale = 0.5f;
	float missileHomingCollisionRadius = 0.5f;
	float missileTrailWidth = 0.5f;
	int missileLifeTime = 240;
	float missileMuzzleOffset = 0.8f;

	std::unique_ptr<Player> player_;

	// 逕ｻ髱｢荳翫↓蟄伜惠縺吶ｋ縺吶∋縺ｦ縺ｮ繝溘し繧､繝ｫ繧堤ｮ｡逅・☆繧九Μ繧ｹ繝・
	std::unique_ptr<MissileManager> missileManager_;


	// 謨ｵ
	std::list<std::unique_ptr<Enemy>> enemies_;
	std::unique_ptr<EnemyBulletManager> enemyBulletManager_;
	std::vector<EnemySpawnData> enemySpawns_;
	std::vector<int> enemyRespawnTimers_;
	EnemyEventManager enemyEventManager_;
	Enemy *lockedEnemy_ = nullptr;


	// 髫懷ｮｳ迚ｩ
	std::list<std::unique_ptr<Obstacle>> obstacles_;

	// ImGui縺ｧ謨ｵ繧貞・縺吶◆繧√・蠎ｧ讓吝､画焚
	float newEnemyPos[3] = { 0.0f, 0.0f, 50.0f };

	// 辷・ｴ繧ｨ繝輔ぉ繧ｯ繝・
	std::unique_ptr<ExplosionManager> explosionManager_;

	// 繧ｲ繝ｼ繝繧ｪ繝ｼ繝舌・貍泌・逕ｨ
	bool isGameOver_ = false;
	int gameOverTimer_ = 0;

	// 繧ｷ繝溘Η繝ｬ繝ｼ繧ｷ繝ｧ繝ｳ繝・・繝ｫUI逕ｨ
	bool showSimulationWindow_ = false;
	int currentSimulationTarget_ = 0;
	std::string simulationSaveMessage_;
	char simulationActionName_[64] = "Action1";
	std::vector<std::string> simulationActionNames_;
	int selectedSimulationActionIndex_ = 0;
	std::string simulationActionMessage_;
	int simulationPlaybackMode_ = 0;
	char missilePresetName_[64] = "MissilePreset1";
	int missilePresetTypeIndex_ = 0;
	std::vector<std::string> missilePresetNames_[2];
	int selectedMissilePresetIndex_[2] = { 0, 0 };
	std::string missilePresetMessage_;

	// =====================================================
	// マネージャークラス
	// =====================================================
	std::unique_ptr<GameCameraManager> cameraManager_;
	std::unique_ptr<LevelManager> levelManager_;
	std::unique_ptr<EnvironmentRenderer> environmentRenderer_;
	std::unique_ptr<GamePlayUIManager> uiManager_;

	// JSON繝輔ぃ繧､繝ｫ縺梧怙蠕後↓譖ｴ譁ｰ縺輔ｌ縺滓律譎ゅｒ險倬鹸縺吶ｋ螟画焚
	std::filesystem::file_time_type lastJsonWriteTime_;
	Enemy* FindAimAssistTarget(Camera* activeCamera) const;
	Enemy* FindMultiLockTarget(Camera* activeCamera) const;
	void BeginMultiLock();
	void PruneMultiLockTargets();
	void UpdateMultiLock(Camera* activeCamera);
	void FireMultiLockMissiles();
	void CancelMultiLock();
	Enemy* aimAssistEnemy_ = nullptr;
	std::vector<Enemy*> multiLockTargets_;
	bool isMultiLockCharging_ = false;
	int multiLockChargeFrames_ = 0;
    Enemy* FindLockOnTarget(Camera* activeCamera) const;
    void UpdateLockOn(Camera* activeCamera, bool shouldUpdateGame);
    bool IsLockedEnemyAlive() const;
};






