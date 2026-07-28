#include "GamePlayScene.h"
#include "SimulationManager.h"
#include "MissilePresetManager.h"
#include "LockOnManager.h"
#include "GamePlaySceneHelpers.h"
#include "3D/ModelManager.h"
#include <Windows.h>
#include "engine/Graphics/DirectXCommon.h"
#include "2D/SpriteCommon.h"
#include "3D/Object3dCommon.h"
#include "engine/Input/Input.h"
#include "engine/Debug/ImGuiManager.h"
#include "engine/Resource/TextureManager.h"
#include <externals/imgui/imgui.h>
#include "engine/Camera/FlyCamera.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Utility/StageLoader.h"
#include "engine/Utility/StageValidation.h"
#include "externals/json.hpp"
#include "Game/editor/EditorReceiver.h"
#include "Game/enemy/Boss.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <shellapi.h>

using json = nlohmann::json;

namespace {
	constexpr int kNoEnemyRespawnTimer = -1;
	constexpr float kSpecialAttackCost = 50.0f;
	constexpr float kSpGaugeRecoveryPerFrame = 3.0f / 60.0f;
	constexpr int kSpecialAttackDurationFrames = 180;
	constexpr int kSpecialAttackFireIntervalFrames = 6;
	constexpr int kNormalMagazineCapacity = 30;
	constexpr int kNormalReserveCapacity = 120;
	constexpr int kHomingMagazineCapacity = 8;
	constexpr int kHomingReserveCapacity = 24;
	constexpr int kReloadDurationFrames = 120;
	constexpr int kKillsPerAmmoPickup = 5;
	constexpr int kPickupNormalAmmo = 60;
	constexpr int kPickupHomingAmmo = 12;
}




GamePlayScene::GamePlayScene(Mode mode)
	: mode_(mode) {
}

bool GamePlayScene::TryConsumeAmmo(MissileType type) {
	if ((type == MissileType::Normal && isNormalReloading_) ||
		(type == MissileType::MissileWithTrail && isHomingReloading_)) {
		return false;
	}
	int &magazine = (type == MissileType::Normal) ? normalAmmoInMagazine_ : homingAmmoInMagazine_;
	if (magazine <= 0) {
		return false;
	}
	--magazine;
	return true;
}

void GamePlayScene::UpdateReload() {
	if (!player_ || player_->IsDead()) return;
	Input *input = Input::GetInstance();
	if (!isNormalReloading_ && input->TriggerKey(DIK_F) &&
		normalAmmoInMagazine_ < kNormalMagazineCapacity && normalAmmoReserve_ > 0) {
		isNormalReloading_ = true;
		normalReloadFrame_ = 0;
	}
	if (!isHomingReloading_ && input->TriggerKey(DIK_G) &&
		homingAmmoInMagazine_ < kHomingMagazineCapacity && homingAmmoReserve_ > 0) {
		isHomingReloading_ = true;
		homingReloadFrame_ = 0;
	}

	auto finishReload = [](int &magazine, int capacity, int &reserve) {
		const int required = capacity - magazine;
		const int loaded = (std::min)(required, reserve);
		magazine += loaded;
		reserve -= loaded;
	};
	if (isNormalReloading_ && ++normalReloadFrame_ >= kReloadDurationFrames) {
		finishReload(normalAmmoInMagazine_, kNormalMagazineCapacity, normalAmmoReserve_);
		isNormalReloading_ = false;
		normalReloadFrame_ = 0;
	}
	if (isHomingReloading_ && ++homingReloadFrame_ >= kReloadDurationFrames) {
		finishReload(homingAmmoInMagazine_, kHomingMagazineCapacity, homingAmmoReserve_);
		isHomingReloading_ = false;
		homingReloadFrame_ = 0;
	}
}

void GamePlayScene::SpawnAmmoPickup(const Vector3 &position) {
	AmmoPickup pickup;
	pickup.basePosition = position;
	pickup.basePosition.y += 2.0f;
	pickup.phase = static_cast<float>(ammoPickups_.size()) * 0.8f;
	pickup.object = std::make_unique<Object3d>();
	pickup.object->Initialize(Object3dCommon::GetInstance());
	pickup.object->SetModel("AmmoPickupSphere");
	pickup.object->SetScale({ 1.2f, 1.2f, 1.2f });
	pickup.object->SetTranslate(pickup.basePosition);
	pickup.object->Update();
	ammoPickups_.push_back(std::move(pickup));
}

void GamePlayScene::UpdateAmmoPickups() {
	if (!player_) return;
	const Vector3 playerPosition = player_->GetPosition();
	for (auto it = ammoPickups_.begin(); it != ammoPickups_.end();) {
		it->phase += 0.05f;
		Vector3 displayPosition = it->basePosition;
		displayPosition.y += std::sin(it->phase) * 0.5f;
		it->object->SetTranslate(displayPosition);
		it->object->SetRotate({ 0.0f, it->phase, 0.0f });
		it->object->Update();

		const float dx = displayPosition.x - playerPosition.x;
		const float dy = displayPosition.y - playerPosition.y;
		const float dz = displayPosition.z - playerPosition.z;
		if (dx * dx + dy * dy + dz * dz <= 9.0f) {
			normalAmmoReserve_ = (std::min)(kNormalReserveCapacity, normalAmmoReserve_ + kPickupNormalAmmo);
			homingAmmoReserve_ = (std::min)(kHomingReserveCapacity, homingAmmoReserve_ + kPickupHomingAmmo);
			it = ammoPickups_.erase(it);
		} else {
			++it;
		}
	}
}


void GamePlayScene::Initialize() {

	//カメラ・シーンリソース
	camera = std::make_unique<Camera>();
	uiManager_ = std::make_unique<GamePlayUIManager>(this);
	environmentRenderer_ = std::make_unique<EnvironmentRenderer>();
	environmentRenderer_->Initialize();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	//スプライト�E初期匁E
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(SpriteCommon::GetInstance() , "resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture(kLockOnReticleTexturePath);
	TextureManager::GetInstance()->LoadTexture(kAimCursorTexturePath);
	TextureManager::GetInstance()->LoadTexture(kBoundaryAlertTexturePath);

	aimCursorSprite_ = std::make_unique<Sprite>();
	aimCursorSprite_->Initialize(SpriteCommon::GetInstance(), kAimCursorTexturePath);
	aimCursorSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	lockOnReticleSprite_ = std::make_unique<Sprite>();
	lockOnReticleSprite_->Initialize(SpriteCommon::GetInstance(), kLockOnReticleTexturePath);
	lockOnReticleSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	TextureManager::GetInstance()->LoadTexture("resources/white1x1.png");
	spGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	spGaugeBackgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	spGaugeFillSprite_ = std::make_unique<Sprite>();
	spGaugeFillSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	spGaugeCostMarkerSprite_ = std::make_unique<Sprite>();
	spGaugeCostMarkerSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");

	const char *hudTexturePaths[] = {
		"resources/hud_panel_frame.png",
		"resources/hud_label_hp.png",
		"resources/hud_label_ammo.png",
		"resources/hud_label_sp.png",
		"resources/hud_digits.png",
		"resources/hud_ammo_icons.png"
	};
	for (const char *path : hudTexturePaths) {
		TextureManager::GetInstance()->LoadTexture(path);
	}
	hudPanelSprite_ = std::make_unique<Sprite>();
	hudPanelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[0]);
	hudAmmoPanelSprite_ = std::make_unique<Sprite>();
	hudAmmoPanelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[0]);
	hpGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	hpGaugeBackgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	hpGaugeFillSprite_ = std::make_unique<Sprite>();
	hpGaugeFillSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	hudHpLabelSprite_ = std::make_unique<Sprite>();
	hudHpLabelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[1]);
	hudAmmoLabelSprite_ = std::make_unique<Sprite>();
	hudAmmoLabelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[2]);
	hudSpLabelSprite_ = std::make_unique<Sprite>();
	hudSpLabelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[3]);
	hudNormalAmmoIconSprite_ = std::make_unique<Sprite>();
	hudNormalAmmoIconSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[5]);
	hudHomingAmmoIconSprite_ = std::make_unique<Sprite>();
	hudHomingAmmoIconSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[5]);
	hudNormalReloadGaugeSprite_ = std::make_unique<Sprite>();
	hudNormalReloadGaugeSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	hudHomingReloadGaugeSprite_ = std::make_unique<Sprite>();
	hudHomingReloadGaugeSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("resources/radar_frame.png");
	radarFrameSprite_ = std::make_unique<Sprite>();
	radarFrameSprite_->Initialize(SpriteCommon::GetInstance(), "resources/radar_frame.png");
	radarFrameSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	radarSweepSprite_ = std::make_unique<Sprite>();
	radarSweepSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	radarSweepSprite_->SetAnchorPoint({ 0.0f, 0.5f });
	for (auto &blipSprite : radarBlipSprites_) {
		blipSprite = std::make_unique<Sprite>();
		blipSprite->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
		blipSprite->SetAnchorPoint({ 0.5f, 0.5f });
	}
	for (auto &digitSprite : hudHpDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[4]);
	}
	for (auto &digitSprite : hudNormalAmmoDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[4]);
	}
	for (auto &digitSprite : hudHomingAmmoDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[4]);
	}
	spGauge_ = 100.0f;
	isSpecialAttackActive_ = false;
	specialAttackFrame_ = 0;
	normalAmmoInMagazine_ = kNormalMagazineCapacity;
	normalAmmoReserve_ = 90;
	homingAmmoInMagazine_ = kHomingMagazineCapacity;
	homingAmmoReserve_ = 16;
	isNormalReloading_ = false;
	isHomingReloading_ = false;
	normalReloadFrame_ = 0;
	homingReloadFrame_ = 0;
	defeatedSmallEnemyCount_ = 0;
	ammoPickups_.clear();

	ModelManager::GetInstance()->CreatePlaneModel("BoundaryAlertPlane");
	Model* alertModel = ModelManager::GetInstance()->FindModel("BoundaryAlertPlane");
	if (alertModel) {
		alertModel->SetTextureFilePath(kBoundaryAlertTexturePath);
		alertModel->SetAlphaReference(0.05f); // Discard almost-black background
	}
	boundaryAlertObject_ = std::make_unique<Object3d>();
	boundaryAlertObject_->Initialize(Object3dCommon::GetInstance());
	boundaryAlertObject_->SetModel("BoundaryAlertPlane");
	ceilingBoundaryAlertObject_ = std::make_unique<Object3d>();
	ceilingBoundaryAlertObject_->Initialize(Object3dCommon::GetInstance());
	ceilingBoundaryAlertObject_->SetModel("BoundaryAlertPlane");

	// SkyboxCommon に DirectX の惁Eを渡して初期化する！E
	// SkyboxCommon is now initialized in Framework.cpp

	// スカイボックスの生Eと初期匁E


	//Model��・パ�EチE��クル
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("multiMesh.obj");
	ModelManager::GetInstance()->CreateSphereModel("Sphere", 16);
	ModelManager::GetInstance()->CreateSphereModel("AmmoPickupSphere", 16);
	if (Model *pickupModel = ModelManager::GetInstance()->FindModel("AmmoPickupSphere")) {
		pickupModel->SetColor({ 0.15f, 1.0f, 0.35f, 1.0f });
	}

	//======================================================
	// プリミティブE生EEE
	//======================================================

	// 地面のモデル
	groundModel = std::make_unique<Object3d>();
	groundModel->Initialize(Object3dCommon::GetInstance());
	groundModel->SetModel("plane.obj");
	groundModel->SetScale({ 3000.0f, 1.0f, 3000.0f });
	groundModel->SetTranslate({ 0.0f, 0.0f, 0.0f });
	objects.push_back(groundModel.get());

	// 琁EE
	myShere = std::make_unique<Primitive>();
	myShere->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Sphere);
	myShere->SetTranslate({ 2.0f,0.0f,0.0f });
	// objects.push_back(myShere.get());
	// ボ�Eンとしても使われるため、目立つように赤くしておく
	if (myShere->GetModel()) {
		myShere->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
	}

	// ボックス
	myBox = std::make_unique<Primitive>();
	myBox->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Box);
	myBox->SetTranslate({ -2.0f,0.0f,0.0f });
	// objects.push_back(myBox.get()); // Boxの代わりにModel��を使ぁE

	// 動的Model��
	myModelObject = std::make_unique<Object3d>();
	myModelObject->Initialize(Object3dCommon::GetInstance());
	ModelManager::GetInstance()->LoadModel("AnimatedCube/AnimatedCube.gltf");
	myModelObject->SetModel("AnimatedCube/AnimatedCube.gltf");
	//objects.push_back(myModelObject.get());

	// アニメーションとノ�Eド階層の読み込み
	animationData = LoadAnimationFile("resources/AnimatedCube", "AnimatedCube.gltf");
	Node rootNode = Model::LoadNodeHierarchy("resources/AnimatedCube", "AnimatedCube.gltf");
	skeleton = CreateSkeleton(rootNode);
	if (!skeleton.joints.empty()) {
		skeleton.joints[skeleton.root].transform.translate = { 0.0f, 0.0f, 0.0f };
	}

	myModelObject->skinCluster = myModelObject->GetModel()->CreateSkinCluster(skeleton);

	// ボ�Eンライン用オブジェクト�E初期匁E
	ModelManager::GetInstance()->CreateLineModel("SkeletonLines");
	skeletonLinesObject = std::make_unique<Object3d>();
	skeletonLinesObject->Initialize(Object3dCommon::GetInstance());
	skeletonLinesObject->SetModel("SkeletonLines");

	// チE��チE��用コライダー表示ラインオブジェクト�E初期匁E
	ModelManager::GetInstance()->CreateLineModel("DebugColliderLines");
	debugColliderLinesObject = std::make_unique<Object3d>();
	debugColliderLinesObject->Initialize(Object3dCommon::GetInstance());
	debugColliderLinesObject->SetModel("DebugColliderLines");

	// チE��チE��用フリーカメラの初期匁E
	debugFlyCamera_ = std::make_unique<FlyCamera>();
	debugFlyCamera_->SetTranslate({ 0.0f, 5.0f, -20.0f }); // 初期位置
	isDebugCameraActive_ = false;


	// リング

	// 部刁E��ング (三日朁E

	// 冁E��エフェクチE

	//パ�EチE��クル
	environmentRenderer_->GetParticleManager()->CreateParticleGroup("test", "resources/circle.png");

	//音声再生
	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");
	songSoundData = AudioManager::GetInstance()->LoadAudio("resources/song_bgm.mp3");

	pVoice1=AudioManager::GetInstance()->PlayWave(soundData1, true);
	pVoice2=AudioManager::GetInstance()->PlayWave(soundData2, true);
	pSongVoice=AudioManager::GetInstance()->PlayWave(songSoundData, true);
	if (pSongVoice) pSongVoice->SetVolume(0.0f);

	// 1. マネージャー経由でトレイル専用Model��を作る
	ModelManager::GetInstance()->CreateTrailModel("SmokeTrail");

	// 2. トレイル計算機�E初期化（今回は60フレーム=紁E秒�Eの長さを残す�E�E
	missileTrail = std::make_unique<Trail>();
	missileTrail->Initialize(60);

	// 3. 描画用オブジェクト�E初期匁E
	trailObject = std::make_unique<Object3d>();
	trailObject->Initialize(Object3dCommon::GetInstance());
	trailObject->SetModel("SmokeTrail");


	ModelManager::GetInstance()->CreateBoxModel("PlayerBox");
	ModelManager::GetInstance()->CreateBoxModel("EnemyBox");
	ModelManager::GetInstance()->CreateBoxModel("BossHull");
	ModelManager::GetInstance()->CreateBoxModel("BossCannon");
	ModelManager::GetInstance()->CreateBoxModel("BossBeam");
	if (Model *model = ModelManager::GetInstance()->FindModel("BossHull")) model->SetColor({ 0.18f, 0.22f, 0.35f, 1.0f });
	if (Model *model = ModelManager::GetInstance()->FindModel("BossCannon")) model->SetColor({ 1.0f, 0.35f, 0.05f, 1.0f });
	if (Model *model = ModelManager::GetInstance()->FindModel("BossBeam")) model->SetColor({ 0.15f, 0.8f, 1.0f, 1.0f });
	ModelManager::GetInstance()->CreateBoxModel("ObstacleBox");

	player_ = std::make_unique<Player>();
	player_->Initialize(kPlayerModelName);

	// 弾
	missileManager_ = std::make_unique<MissileManager>();
	missileManager_->Initialize();

	// 爁E��エフェクチE
	explosionManager_ = std::make_unique<ExplosionManager>();
	explosionManager_->Initialize(environmentRenderer_->GetParticleManager());

	//敵
	enemies_.clear();
	auto firstEnemy = std::make_unique<Enemy>();
	firstEnemy->Initialize({ 0.0f, 0.0f, 50.0f });
	enemies_.push_back(std::move(firstEnemy));

	enemyBulletManager_ = std::make_unique<EnemyBulletManager>();
	enemyBulletManager_->Initialize();

	simulationManager_ = std::make_unique<SimulationManager>(this);
	missilePresetManager_ = std::make_unique<MissilePresetManager>(this);
	lockOnManager_ = std::make_unique<LockOnManager>(this);

	// ゲームオーバ�E演�Eの初期匁E
	isGameOver_ = false;
	gameOverTimer_ = 0;

	ReloadSceneJson();
// 	simulationManager_->RefreshSimulationActionNames();
// 	missilePresetManager_->RefreshMissilePresetNames();

	if (IsSimulationMode()) {
		isEditorPreviewPlaying_ = false;
		uiManager_->currentSimulationTarget_ = 2;
		uiManager_->showSimulationWindow_ = true;
		SetDebugCameraActive(true);
	}

	// エチE��ターレシーバ�Eの初期匁E
	EditorReceiver::GetInstance()->Initialize();
}

void GamePlayScene::SetDebugCameraActive(bool isActive) {
	if (isDebugCameraActive_ == isActive) {
		return;
	}

	isDebugCameraActive_ = isActive;
	isCinematicLockOnCameraInitialized_ = false;
	if (isDebugCameraActive_) {
		debugFlyCamera_->SetTranslate(camera->GetTranslate());
		Object3dCommon::GetInstance()->SetDefaultCamera(debugFlyCamera_.get());
		OutputDebugStringA("[DebugCamera] ON: FlyCamera\n");
	} else {
		Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
		OutputDebugStringA("[DebugCamera] OFF: Player Camera\n");
	}
}

void GamePlayScene::ReloadSceneJson() {
	bossSpawned_ = false;
	lockedEnemy_ = nullptr;
	aimAssistEnemy_ = nullptr;
	if (lockOnManager_) {
		lockOnManager_->CancelMultiLock();
	}
	isCinematicLockOnCameraInitialized_ = false;
	enemies_.clear();
	obstacles_.clear();
	enemySpawns_.clear();

	StageLoader::LoadSceneJson("resources/scene.json", enemies_, obstacles_, player_.get(), &enemySpawns_);
	enemyRespawnTimers_.assign(enemySpawns_.size(), kNoEnemyRespawnTimer);

	// イベントデータを�E読み込み
	enemyEventManager_.LoadEvents("resources/enemy_events.json");
	for (auto& spawnData : enemySpawns_) {
		if (spawnData.HasReinforcementTrigger() || enemyEventManager_.IsTargetEnemy(spawnData.name)) {
			spawnData.isInitialSpawn = false;
		}
	}

	for (size_t i = 0; i < enemySpawns_.size(); ++i) {
		if (enemySpawns_[i].isInitialSpawn) {
			SpawnEnemyFromSpawnPoint(i);
		}
	}

	try {
		lastJsonWriteTime_ = std::filesystem::last_write_time("resources/scene.json");
	} catch (...) {
		// JSONがまだ存在しなぁE��合でも、エチE��タ操作を続けられるよぁE��する"
	}
}

void GamePlayScene::ResetEditorPreview() {
	isEditorPreviewPlaying_ = false;
	isGameOver_ = false;
	gameOverTimer_ = 0;
	lockedEnemy_ = nullptr;
	aimAssistEnemy_ = nullptr;
	if (lockOnManager_) {
		lockOnManager_->CancelMultiLock();
	}
	isCinematicLockOnCameraInitialized_ = false;

	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	if (player_) {
		player_->Initialize(kPlayerModelName);
	}
	if (missileManager_) {
		missileManager_->Initialize();
	}
	if (enemyBulletManager_) {
		enemyBulletManager_->Initialize();
	}
	if (explosionManager_) {
		explosionManager_->Initialize(environmentRenderer_->GetParticleManager());
	}

	ReloadSceneJson();

	if (!isDebugCameraActive_ && player_) {
		Vector3* targetPos = nullptr;
		Vector3 enemyPos;
		if (lockedEnemy_) {
			enemyPos = lockedEnemy_->GetPosition();
			targetPos = &enemyPos;
		}
		player_->UpdateCamera(camera.get(), targetPos);
	}

	OutputDebugStringA("[EditorPreview] Reset scene and paused.\n");
}

void GamePlayScene::SpawnEnemyFromSpawnPoint(size_t spawnPointIndex) {
	if (spawnPointIndex >= enemySpawns_.size()) {
		return;
	}

	const EnemySpawnData &spawnData = enemySpawns_[spawnPointIndex];
	auto enemy = std::make_unique<Enemy>();
	enemy->Initialize(spawnData.position);
	enemy->SetRotation(spawnData.rotation);
	if (spawnData.flightPath.IsValid()) {
		enemy->SetFlightPath(spawnData.flightPath.points, spawnData.flightPath.loop, spawnData.flightPath.speed);
	}
	enemy->SetSpawnPointIndex(spawnPointIndex);
	enemies_.push_back(std::move(enemy));

	if (spawnPointIndex < enemyRespawnTimers_.size()) {
		enemyRespawnTimers_[spawnPointIndex] = kNoEnemyRespawnTimer;
	}
}

bool GamePlayScene::IsEnemySpawnPointActive(size_t spawnPointIndex) const {
	for (const auto &enemy : enemies_) {
		if (enemy && enemy->GetSpawnPointIndex() == spawnPointIndex && !enemy->IsDead()) {
			return true;
		}
	}
	return false;
}

void GamePlayScene::ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames) {
	if (spawnPointIndex >= enemySpawns_.size()) {
		return;
	}
	if (enemyRespawnTimers_.size() < enemySpawns_.size()) {
		enemyRespawnTimers_.resize(enemySpawns_.size(), kNoEnemyRespawnTimer);
	}
	if (enemyRespawnTimers_[spawnPointIndex] != kNoEnemyRespawnTimer || IsEnemySpawnPointActive(spawnPointIndex)) {
		return;
	}

	enemyRespawnTimers_[spawnPointIndex] = delayFrames > 0 ? delayFrames : 1;
}

void GamePlayScene::TriggerEnemyReinforcements(const std::string &deadEnemyName) {
	if (deadEnemyName.empty()) {
		return;
	}

	const std::vector<EnemyEvent> events = enemyEventManager_.GetEventsForTrigger(deadEnemyName);
	for (const EnemyEvent &event : events) {
		for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
			if (enemySpawns_[spawnPointIndex].name == event.targetEnemyName) {
				ScheduleEnemySpawn(spawnPointIndex, event.delayFrames);
				break;
			}
		}
	}

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
		EnemySpawnData &spawnData = enemySpawns_[spawnPointIndex];
		if (!spawnData.remainingReinforcementTriggers.empty()) {
			auto it = std::find(spawnData.remainingReinforcementTriggers.begin(), spawnData.remainingReinforcementTriggers.end(), deadEnemyName);
			if (it != spawnData.remainingReinforcementTriggers.end()) {
				spawnData.remainingReinforcementTriggers.erase(it);
				if (spawnData.remainingReinforcementTriggers.empty()) {
					ScheduleEnemySpawn(spawnPointIndex, spawnData.reinforcementDelayFrames);
				}
			}
		}
	}
}

void GamePlayScene::UpdateEnemyRespawns() {
	if (enemyRespawnTimers_.size() < enemySpawns_.size()) {
		enemyRespawnTimers_.resize(enemySpawns_.size(), kNoEnemyRespawnTimer);
	}

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemyRespawnTimers_.size(); ++spawnPointIndex) {
		int &timer = enemyRespawnTimers_[spawnPointIndex];
		if (timer == kNoEnemyRespawnTimer) {
			continue;
		}
		if (timer > 0) {
			--timer;
		}
		if (timer == 0) {
			SpawnEnemyFromSpawnPoint(spawnPointIndex);
		}
	}
}

bool GamePlayScene::HasPendingEnemySpawns() const {
	for (int timer : enemyRespawnTimers_) {
		if (timer != kNoEnemyRespawnTimer) {
			return true;
		}
	}
	return false;
}





























MissileTuning GamePlayScene::MakeMissileTuning(MissileType type) const {
	MissileTuning tuning;
	if (type == MissileType::Normal) {
		tuning.speed = missileNormalSpeed;
		tuning.homingStrength = 0.0f;
		tuning.scale = missileNormalScale;
		tuning.collisionRadius = missileNormalCollisionRadius;
		tuning.trailWidth = 0.0f;
		tuning.lifeTime = missileNormalLifeTime;
		return tuning;
	}

	tuning.speed = missileSpeed;
	tuning.homingStrength = missileHomingStrength;
	tuning.scale = missileHomingScale;
	tuning.collisionRadius = missileHomingCollisionRadius;
	tuning.trailWidth = missileTrailWidth;
	tuning.lifeTime = missileLifeTime;
	return tuning;
}































void GamePlayScene::Finalize() {
	if (pVoice1) {
		pVoice1->Stop();
		pVoice1->DestroyVoice();
		pVoice1 = nullptr;
	}
	if (pVoice2) {
		pVoice2->Stop();
		pVoice2->DestroyVoice();
		pVoice2 = nullptr;
	}
	if (pSongVoice) {
		pSongVoice->Stop();
		pSongVoice->DestroyVoice();
		pSongVoice = nullptr;
	}

	AudioManager::GetInstance()->UnloadWave(soundData1);
	AudioManager::GetInstance()->UnloadWave(soundData2);
	AudioManager::GetInstance()->UnloadWave(songSoundData);

	// シーン刁E��替え時にポストエフェクトを通常に戻ぁE
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	EditorReceiver::GetInstance()->Finalize();
}

void GamePlayScene::Update() {

	// BlenderからチE�Eタが来てぁE��ら敵をリアルタイム更新�E�E
	if (EditorReceiver::GetInstance()->Update(player_.get(), enemies_, obstacles_, enemySpawns_)) {
		for (auto &spawnData : enemySpawns_) {
			if (spawnData.HasReinforcementTrigger() || enemyEventManager_.IsTargetEnemy(spawnData.name)) {
				spawnData.isInitialSpawn = false;
			}
		}
// 		SpawnEnemiesFromSpawnPoints();
	}


	// =========================================================
	// ホットリロード�E監視�E琁E��E
	// =========================================================
	try {
		// 今�E "scene.json" の更新日時をチェチE��する
		auto currentTime = std::filesystem::last_write_time("resources/scene.json");

		// もし記�EしてぁE��日時よりも新しけれ�E�E�＝Blenderで上書き保存されたら！E��E
		if (currentTime > lastJsonWriteTime_) {
			ReloadSceneJson();

			// チE��チE��ウィンドウにお知らせを�EぁE
			OutputDebugStringA("Hot Reloaded: scene.json を�E読み込みしました�E�\n");
		}
	} catch (...) {
		// 💡趁E��要E��Blenderがファイルに書き込んでぁE��最中�E�数ミリ秒）�E
		// C++からアクセスできずエラーになることがあるため、try-catchで握りつぶぁE
	}

	const bool canUseKeyboardInput = !IsImGuiKeyboardCaptureActive();
	const bool canUseMouseInput = !IsImGuiMouseCaptureActive();

	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_0)) {
		OutputDebugStringA("HIt 0\n");
	}

	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_F2)) {
		if (IsSimulationMode()) {
			PostQuitMessage(0);
		} else {
			LaunchSimulationExecutable();
		}
		return;
	}

	if (canUseKeyboardInput && IsSimulationMode() && Input::GetInstance()->TriggerKey(DIK_F3)) {
		SetDebugCameraActive(!isDebugCameraActive_);
	}

	// Rキーでシーンを最初からやり直ぁE
	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_R)) {
		SceneManager::GetInstance()->ChangeScene(IsSimulationMode() ? "SIMULATION" : "GAMEPLAY");
		return;
	}

	// ==========================================
	// ゲームオーバE判定と演E進衁E
	// ==========================================
	if (!IsSimulationMode() && !isGameOver_ && player_ && player_->IsDead()) {
		isGameOver_ = true;
		gameOverTimer_ = 0;

		std::vector<Vector3> playerHitPos = { player_->GetPosition() };
		if (explosionManager_) {
			explosionManager_->CreateDestructionEffects(playerHitPos);
		}

		if (pVoice2) {
			pVoice2->Stop();
		}
	}

	bool shouldUpdateGame = true;

	if (isGameOver_) {
		gameOverTimer_++;

		// 絶望E白黒化EグレースケールEエフェクトを適用EE
		if (PostEffect::GetInstance()) {
			float effectProgress = static_cast<float>(gameOverTimer_) / 120.0f;
			if (effectProgress > 1.0f) {
				effectProgress = 1.0f;
			}
			float vignetteRadius = 0.62f - 0.22f * effectProgress;
			float blurIntensity = 1.5f + 3.0f * effectProgress;
			PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.38f, blurIntensity);
		}

		// 5フレームに1回だけ更新することで、スローモーションE世界停止Eを実現EE
		shouldUpdateGame = (gameOverTimer_ % 5 == 0);

		// 紁E秒！E20フレームE経過したら、正式にゲームオーバEシーンへ遷移するEE
		if (gameOverTimer_ >= 120) {
			SceneManager::GetInstance()->ChangeScene("GAMEOVER");
		}
	} else {
		// 通常時：ノーマルエフェクト、またはブースト時のスピード演出
		if (PostEffect::GetInstance()) {
			bool isBoosting = false;
			if (player_ && player_->GetCurrentMode() == PlayerMode::Fighter) {
				float maxSpeed = player_->GetModeParams(PlayerMode::Fighter).maxMoveSpeed;
				float speed = MyMath::Length(player_->GetVelocity());
				if (speed > maxSpeed * 1.5f) {
					isBoosting = true;
					float effectProgress = std::clamp((speed - maxSpeed * 1.5f) / (maxSpeed * 3.0f - maxSpeed * 1.5f), 0.0f, 1.0f);
					float vignetteRadius = 0.5f - 0.1f * effectProgress; // 視界の狭まりを控えめに
					float blurIntensity = effectProgress * 0.5f; // ブラーをかなり弱くして前が見えるようにする
					PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.4f, blurIntensity);
				}
			}
			
			if (player_) {
				if (previousPlayerHP_ == -1) {
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() < previousPlayerHP_) {
					damageEffectTimer_ = 30;
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() > previousPlayerHP_) {
					previousPlayerHP_ = player_->GetHP();
				}
			}

			if (damageEffectTimer_ > 0) {
				damageEffectTimer_--;
				PostEffect::GetInstance()->SetEffectType(13); // Fold Wave
			} else if (!isBoosting) {
				PostEffect::GetInstance()->SetEffectType(0); // 0: Normal
			}
		}
	}
	shouldUpdateGame = shouldUpdateGame && isEditorPreviewPlaying_;
	const bool isSimulation = IsSimulationMode();
	const bool isFullFlowPreview = !isSimulation || uiManager_->simulationPlaybackMode_ == 1;
	const bool isSelectedOnlyPreview = isSimulation && uiManager_->simulationPlaybackMode_ == 0;
	const bool updateSelectedPlayer = shouldUpdateGame && canUseKeyboardInput && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 0);
	const bool updateSelectedMissiles = shouldUpdateGame && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 1);
	const bool updateSelectedEnemies = shouldUpdateGame && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 2);
	const bool updateSelectedParticles = shouldUpdateGame && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 3);
	const bool allowMouseMissileFire = shouldUpdateGame && canUseMouseInput && (!isSimulation || isFullFlowPreview);
	const bool allowLockOnBehavior = !isGameOver_ && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 1 || uiManager_->currentSimulationTarget_ == 2);
	const bool updateDebugWireframes = !isSimulation || isFullFlowPreview || updateSelectedPlayer || updateSelectedMissiles || updateSelectedEnemies || updateSelectedParticles;
	const bool updateAnimationPreview = !isSimulation || isFullFlowPreview;

	const bool isAnimationEditor = isSimulation && uiManager_->currentSimulationTarget_ == 5;
	static bool wasAnimationEditor = false;
	if (isAnimationEditor && !wasAnimationEditor) {
		SetDebugCameraActive(true);
		if (player_ && debugFlyCamera_) {
			Vector3 pPos = player_->GetPosition();
			// プレイヤーの少し後ろ、やや上から見下ろすように配置 (中心に捉える)
			debugFlyCamera_->SetTranslate({ pPos.x, pPos.y + 2.0f, pPos.z - 12.0f });
			debugFlyCamera_->SetQuaternion({ 0.0f, 0.0f, 0.0f, 1.0f });
		}
	} else if (!isAnimationEditor && wasAnimationEditor) {
		SetDebugCameraActive(false);
	}
	wasAnimationEditor = isAnimationEditor;

	if (shouldUpdateGame && canUseKeyboardInput && !isSpecialAttackActive_) {
		UpdateReload();
	}

	// CキーでSPを50%消費し、3秒間の連射必殺技を発動する。
	if (!isGameOver_ && shouldUpdateGame && canUseKeyboardInput &&
		!isSpecialAttackActive_ && spGauge_ >= kSpecialAttackCost &&
		Input::GetInstance()->TriggerKey(DIK_C)) {
		spGauge_ -= kSpecialAttackCost;
		isSpecialAttackActive_ = true;
		specialAttackFrame_ = 0;
		if (player_) {
			player_->SetSpecialAttackActive(true);
		}
	}

	// Vキーで歌システム（ルンピカゲージ100%消費）を発動する。
	if (!isGameOver_ && shouldUpdateGame && canUseKeyboardInput &&
		!isSongActive_ && songGauge_ >= 100.0f &&
		Input::GetInstance()->TriggerKey(DIK_V)) {
		songGauge_ = 0.0f;
		isSongActive_ = true;
		songFrame_ = 0;
		if (player_) {
			player_->SetSongActive(true);
		}
		if (pVoice2) pVoice2->SetVolume(0.0f);
		if (pSongVoice) pSongVoice->SetVolume(1.0f);
	}

	if (isSongActive_) {
		++songFrame_;
		if (songFrame_ >= 900) { // 15 seconds
			isSongActive_ = false;
			songFrame_ = 0;
			if (player_) {
				player_->SetSongActive(false);
			}
			if (pVoice2) pVoice2->SetVolume(1.0f);
			if (pSongVoice) pSongVoice->SetVolume(0.0f);
		}
	}

	if (isSpecialAttackActive_) {
		if (specialAttackFrame_ % kSpecialAttackFireIntervalFrames == 0 && missilePresetManager_) {
			// 通常弾と誘導弾を操作中の照準方向へ同時発射する。
			missilePresetManager_->FirePlayerMissile(MissileType::Normal, nullptr, -0.3f);
			// 初速は照準方向にして前方へ射出し、直進区間の後に更新側で敵を捕捉する。
			missilePresetManager_->FirePlayerMissile(MissileType::MissileWithTrail, nullptr, 0.3f);
		}
		++specialAttackFrame_;
		if (specialAttackFrame_ >= kSpecialAttackDurationFrames) {
			isSpecialAttackActive_ = false;
			specialAttackFrame_ = 0;
			if (player_) {
				player_->SetSpecialAttackActive(false);
			}
		}
	} else {
		spGauge_ = std::clamp(spGauge_ + kSpGaugeRecoveryPerFrame, 0.0f, 100.0f);
	}

	if (myBox && animationData.duration > 0.0f) {
		if (playAnimation && updateAnimationPreview) {
			animationTime += 1.0f / 60.0f;
			animationTime = std::fmod(animationTime, animationData.duration);
		}
		
		// アニメーションの更新と骨への適用
		ApplyAnimation(skeleton, animationData, animationTime);
		::Update(skeleton);
		if (enableSkinning && myModelObject->GetModel()) {
			myModelObject->GetModel()->UpdateSkinCluster(myModelObject->skinCluster, skeleton);
		}

		// 今EめEに合わせた状態で使ぁEめ、Skeletonから計算結果を取りEしてBox/Modelに適用する
		if (!skeleton.joints.empty()) {
			myBox->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
			myBox->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
			myBox->SetScale(skeleton.joints[skeleton.root].transform.scale);

			// スキニングが実裁Eれたため、スキンなしModelの場合EみTransformを適用する
			// スキニングが実裁Eれたため、スキンなしModelの場合EみTransformを適用する
			if (myModelObject->GetModel()) {
				if (!myModelObject->skinCluster.isValid) {
					myModelObject->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
					myModelObject->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
					myModelObject->SetScale(skeleton.joints[skeleton.root].transform.scale);
				} else {
					// スキニングModelはアニメーションが行Eに含まれるため、EースのトランスフォームはリセチEする
					// (これを行わなぁE二重に移動して画面外に消えめE
					myModelObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
					myModelObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
					myModelObject->SetScale({ modelScale, modelScale, modelScale });
				}
			}
		}

		// 骨描画の更新
		bool isAnimationEditor = IsSimulationMode() && uiManager_ && uiManager_->currentSimulationTarget_ == 5;
		if ((showBones || isAnimationEditor) && player_) {
			std::vector<VertexData> lineVertices;

			const Skeleton& playerSkeleton = player_->GetSkeleton();
			for (size_t i = 0; i < playerSkeleton.joints.size(); ++i) {
				Vector3 pos = {
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][0],
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][1],
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][2]
				};

				// ライン用の頂点を作EE親がいる場合！E
				if (playerSkeleton.joints[i].parent) {
					int32_t parentIndex = *playerSkeleton.joints[i].parent;
					Vector3 parentPos = {
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][0],
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][1],
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][2]
					};

					VertexData v1, v2;
					v1.position = { parentPos.x, parentPos.y, parentPos.z, 1.0f };
					v1.normal = { 0.0f, 1.0f, 0.0f };
					v1.texcoord = { 0.0f, 0.0f };

					v2.position = { pos.x, pos.y, pos.z, 1.0f };
					v2.normal = { 0.0f, 1.0f, 0.0f };
					v2.texcoord = { 1.0f, 1.0f };

					Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色
					if (simulationManager_ && simulationManager_->IsBoneSelected(playerSkeleton.joints[i].name)) {
						v1.color = { 1.0f, 1.0f, 0.0f, 1.0f };
						v2.color = { 1.0f, 1.0f, 0.0f, 1.0f };
					} else {
						v1.color = color;
						v2.color = color;
					}

					lineVertices.push_back(v1);
					lineVertices.push_back(v2);
				}
			}

			// ラインModel��の頂点を更新
			// ラインModelの頂点を更新
			if (player_ && player_->GetObject3d()) {
				skeletonLinesObject->SetTranslate(player_->GetPosition());
				skeletonLinesObject->SetQuaternionRotate(player_->GetQuaternion());
				skeletonLinesObject->SetScale(player_->GetObject3d()->GetScale());
			}
			if (!lineVertices.empty() && skeletonLinesObject->GetModel()) {
				skeletonLinesObject->GetModel()->UpdateLineVertices(lineVertices);
			}
			skeletonLinesObject->Update();
		}
	}

	// Model��の更新
	if (showModel && myModelObject) {
		myModelObject->Update();
	}

	// Debug camera switching is handled by ImGui buttons in UpdateUI().
	if (false && Input::GetInstance()->TriggerKey(DIK_F1)) {
		SetDebugCameraActive(!isDebugCameraActive_);
	}

	// プレイヤーの移動とカメラ更新より先にロックオン状態を確定する。
	// これにより、ロックオンしたフレームから機体の追従方向とカメラ方向が一致する。
	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	lockOnManager_->UpdateLockOn(activeCamera, allowLockOnBehavior);

	// プレイヤーの更新と、カメラの追征E
	if (player_) {
		if (updateSelectedPlayer) {
			Vector3 lockOnTargetPosition;
			const Vector3 *lockOnTarget = nullptr;
			if (lockedEnemy_) {
				lockOnTargetPosition = lockedEnemy_->GetPosition();
				lockOnTarget = &lockOnTargetPosition;
			}
			player_->Update(obstacles_, lockOnTarget);
		} else {
			player_->UpdateModel();
		}

	}

	// ==========================================
	// 敵
	// ==========================================
	// プレイヤーの最新座標を取得すめE
	Vector3 playerPos = player_ ? player_->GetOBB().center : Vector3{ 0.0f, 0.0f, 0.0f };

	if (updateSelectedEnemies) {
		// 敵の弾の更新�E�被弾時�E爁E��座標を受け取る�E�E
		std::vector<Vector3> enemyBulletHits;
		if (enemyBulletManager_ && player_) {
			enemyBulletManager_->Update(player_.get(), enemyBulletHits, obstacles_);
		}

		// 敵の弾が�Eレイヤーに当たった場合も爁E��を発生させる
		if (explosionManager_ && !enemyBulletHits.empty()) {
			explosionManager_->CreateHitEffects(enemyBulletHits);
		}

		for (auto it = enemies_.begin(); it != enemies_.end(); ) {
			(*it)->Update(playerPos, enemyBulletManager_.get(), obstacles_);
			if (Boss *boss = dynamic_cast<Boss *>(it->get())) {
				const int summonCount = boss->ConsumeSummonRequests();
				for (int i = 0; i < summonCount; ++i) {
					auto escort = std::make_unique<Enemy>();
					const float side = static_cast<float>(i - summonCount / 2) * 8.0f;
					escort->Initialize({ boss->GetPosition().x + side, boss->GetPosition().y - 3.0f, boss->GetPosition().z - 12.0f });
					escort->StartChasingPlayer();
					enemies_.push_back(std::move(escort));
				}
			}
			if ((*it)->IsDead()) {
				const bool defeatedBoss = (*it)->IsBoss();
				const Vector3 defeatedPosition = (*it)->GetPosition();
				if (!defeatedBoss) {
					++defeatedSmallEnemyCount_;
					if (defeatedSmallEnemyCount_ % kKillsPerAmmoPickup == 0) {
						SpawnAmmoPickup(defeatedPosition);
					}
				}
				if (lockedEnemy_ == it->get()) {
					lockedEnemy_ = nullptr;
					isCinematicLockOnCameraInitialized_ = false;
				}
				if (aimAssistEnemy_ == it->get()) {
					aimAssistEnemy_ = nullptr;
				}
				if (missileManager_) {
// 					missileManager_->ClearTarget(it->get());
				}
				size_t spawnPointIndex = (*it)->GetSpawnPointIndex();
				
				if (spawnPointIndex < enemySpawns_.size()) {
					const std::string& deadName = enemySpawns_[spawnPointIndex].name;
					TriggerEnemyReinforcements(deadName);
				}

				if (spawnPointIndex < enemySpawns_.size() && enemySpawns_[spawnPointIndex].isInitialSpawn) {
// 					ScheduleEnemySpawn(spawnPointIndex, kEnemyRespawnDelayFrames);
				}
				songGauge_ = (std::min)(songGauge_ + 20.0f, 100.0f);
				it = enemies_.erase(it); // 当たった敵はリストから消去滁E

				// ボスが召喚した雑魚敵が残っていても、ボス本体を倒した時点でクリアにする。
				if (defeatedBoss && !IsSimulationMode() && !isGameOver_) {
					SceneManager::GetInstance()->ChangeScene("CLEAR");
					return;
				}
			} else {
				++it;
			}
		}
		UpdateAmmoPickups();
		UpdateEnemyRespawns();

		if (!IsSimulationMode() && !isGameOver_ && enemies_.empty() && !HasPendingEnemySpawns()) {
			if (!bossSpawned_) {
				auto boss = std::make_unique<Boss>();
				boss->Initialize({ playerPos.x, playerPos.y + 18.0f, playerPos.z + 90.0f });
				enemies_.push_back(std::move(boss));
				bossSpawned_ = true;
			} else {
				SceneManager::GetInstance()->ChangeScene("CLEAR");
				return;
			}
		}

		// 障害物自身のUpdateを回す（現状中身は空に近いですが一応回します！）
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	} else {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
	}

	// カメラの更新
	if (isDebugCameraActive_) {
		debugFlyCamera_->SetCanUseKeyboard(canUseKeyboardInput);
		debugFlyCamera_->Update(); // FlyCameraが内部でマウスホバー判定を行って更新する
		
		if (isAnimationEditor && simulationManager_) {
			simulationManager_->UpdateShortcuts();
			ImGuiIO& io = ImGui::GetIO();
			Vector2 localMousePos;
			
			static bool s_clickedOnBone = false;
			static bool s_isDraggingBone = false;
			static ImVec2 s_mouseDownPos = {0, 0};
			
			if (FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, localMousePos)) {
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					float gw, gh;
					FlyCamera::GetGameViewSize(gw, gh);
					Ray ray = MyMath::ScreenToRay(localMousePos, gw, gh, MyMath::Inverse(debugFlyCamera_->GetViewProjectionMatrix()));
					
					float closestDist = 999999.0f;
					std::string closestBone = "";
					
					if (this->player_) {
						const Skeleton& playerSkeleton = this->player_->GetSkeleton();
						for (const auto& joint : playerSkeleton.joints) {
							Vector3 pos = {
								joint.skeletonSpaceMatrix.m[3][0],
								joint.skeletonSpaceMatrix.m[3][1],
								joint.skeletonSpaceMatrix.m[3][2]
							};
						
							Vector3 currentScale = {1.0f, 1.0f, 1.0f};
							if (this->player_ && this->player_->GetObject3d()) {
								currentScale = this->player_->GetObject3d()->GetScale();
								Matrix4x4 worldMat = MyMath::MakeAffineMatrix(currentScale, this->player_->GetQuaternion(), this->player_->GetPosition());
								pos = MyMath::Transform(pos, worldMat);
							}

							Sphere s;
							s.center = pos;
							s.radius = 0.5f * currentScale.x; // 判定半径を少し小さめに調整
						
						float dist;
						if (MyMath::IntersectRaySphere(ray, s, &dist)) {
							if (dist < closestDist) {
								closestDist = dist;
								closestBone = joint.name;
							}
						}
					}
					}
					
					s_clickedOnBone = !closestBone.empty();
					s_mouseDownPos = io.MousePos;

					if (s_clickedOnBone) {
						if (io.KeyShift) {
							if (simulationManager_->IsBoneSelected(closestBone)) {
								simulationManager_->RemoveSelectedBoneName(closestBone);
							} else {
								simulationManager_->AddSelectedBoneName(closestBone);
							}
						} else {
							// すでに選択済みのボーンをクリックした場合は、複数選択を維持してドラッグできるようにする
							if (!simulationManager_->IsBoneSelected(closestBone)) {
								simulationManager_->ClearSelectedBones();
								simulationManager_->AddSelectedBoneName(closestBone);
							}
						}
						isBoxSelecting_ = false;
						s_isDraggingBone = true;
					} else {
						// 何もない空間をクリックした時
						if (io.KeyShift || simulationManager_->GetSelectedBoneNames().empty()) {
							// Shiftキーを押しているか、何も選択されていない場合はボックス選択を開始
							isBoxSelecting_ = true;
							s_isDraggingBone = false;
							boxSelectStartPos_ = localMousePos;
							boxSelectEndPos_ = localMousePos;
						} else {
							// すでにボーンが選択されている場合、ドラッグで回転できるように待機
							isBoxSelecting_ = false;
							s_isDraggingBone = true;
						}
					}
				}
				
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					const auto& selBones = simulationManager_->GetSelectedBoneNames();
					float dx = io.MouseDelta.x;
					float dy = io.MouseDelta.y;
					
					if (isBoxSelecting_) {
						Vector2 localMousePos;
						if (FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, localMousePos)) {
							boxSelectEndPos_ = localMousePos;
						}
					} else if (s_isDraggingBone && !selBones.empty()) {
						if (dx != 0.0f || dy != 0.0f) {
							bool doTranslate = io.KeyCtrl;
							for (const auto& boneName : selBones) {
								if (doTranslate) {
									simulationManager_->AddBoneTranslationFromDrag(boneName, dx, dy);
								} else {
									simulationManager_->AddBoneRotationFromDrag(boneName, dx, dy);
								}
							}
						}
					}
				}

				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					if (isBoxSelecting_) {
						isBoxSelecting_ = false;
						
						float minX = (std::min)(boxSelectStartPos_.x, boxSelectEndPos_.x);
						float maxX = (std::max)(boxSelectStartPos_.x, boxSelectEndPos_.x);
						float minY = (std::min)(boxSelectStartPos_.y, boxSelectEndPos_.y);
						float maxY = (std::max)(boxSelectStartPos_.y, boxSelectEndPos_.y);
						
						if (maxX - minX > 2.0f && maxY - minY > 2.0f) {
							if (this->player_) {
								float gw, gh;
								FlyCamera::GetGameViewSize(gw, gh);
								Matrix4x4 vpMat = debugFlyCamera_->GetViewProjectionMatrix();
								const Skeleton& playerSkeleton = this->player_->GetSkeleton();
								
								for (const auto& joint : playerSkeleton.joints) {
									Vector3 pos = {
										joint.skeletonSpaceMatrix.m[3][0],
										joint.skeletonSpaceMatrix.m[3][1],
										joint.skeletonSpaceMatrix.m[3][2]
									};
									
									if (this->player_->GetObject3d()) {
										Vector3 currentScale = this->player_->GetObject3d()->GetScale();
										Matrix4x4 worldMat = MyMath::MakeAffineMatrix(currentScale, this->player_->GetQuaternion(), this->player_->GetPosition());
										pos = MyMath::Transform(pos, worldMat);
									}
									
									Vector3 screenPos = MyMath::WorldToScreen(pos, vpMat, gw, gh);
									
									if (screenPos.z > 0.0f && screenPos.z < 1.0f) {
										if (screenPos.x >= minX && screenPos.x <= maxX &&
											screenPos.y >= minY && screenPos.y <= maxY) {
											simulationManager_->AddSelectedBoneName(joint.name);
										}
									}
								}
							}
						}
					} else {
						// ドラッグせずにクリックだけで離した場合の選択解除処理
						if (!s_clickedOnBone && !io.KeyShift) {
							ImVec2 dragDelta(io.MousePos.x - s_mouseDownPos.x, io.MousePos.y - s_mouseDownPos.y);
							if (std::abs(dragDelta.x) < 2.0f && std::abs(dragDelta.y) < 2.0f) {
								simulationManager_->ClearSelectedBones();
							}
						}
					}
					s_isDraggingBone = false;
				}

				if (isBoxSelecting_) {
					float minViewX, minViewY, maxViewX, maxViewY;
					if (FlyCamera::GetGameViewBounds(minViewX, minViewY, maxViewX, maxViewY)) {
						ImVec2 start(minViewX + boxSelectStartPos_.x, minViewY + boxSelectStartPos_.y);
						ImVec2 end(minViewX + boxSelectEndPos_.x, minViewY + boxSelectEndPos_.y);
						ImU32 colFill = IM_COL32(100, 150, 250, 80);
						ImU32 colBorder = IM_COL32(150, 200, 255, 200);
						ImGui::GetForegroundDrawList()->AddRectFilled(start, end, colFill);
						ImGui::GetForegroundDrawList()->AddRect(start, end, colBorder, 0.0f, 0, 1.5f);
					}
				}
			}
		}
	} else {
		if (player_) {
			Vector3* targetPos = nullptr;
			Vector3 enemyPos;
			if (lockedEnemy_) {
				enemyPos = lockedEnemy_->GetPosition();
				targetPos = &enemyPos;
			}
			player_->UpdateCamera(camera.get(), targetPos);
		}
		camera->Update();
	}

	if (isSelectedOnlyPreview) {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
		if (enemyBulletManager_) {
			enemyBulletManager_->UpdateModels();
		}
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	}

	for (Object3d *object3d : objects) {
		object3d->Update();
	}

	Vector2 size = sprite->GetSize();
	size.x = 300.0f;
	size.y = 300.0f;
	sprite->SetSize(size);

	if (environmentRenderer_->GetShowParticles() && updateSelectedParticles) {
	}


	// ==========================================
	// ミサイルの発封E�E琁E
	// ==========================================
	if (allowMouseMissileFire && player_ && !isGameOver_ && !isSpecialAttackActive_) {
		Input *input = Input::GetInstance();
		// 左クリチE���E�速くて煙が出なぁE��常弾
		if (input->TriggerMouseButton(0)) {
			Enemy* aimTarget = nullptr;
			if (lockedEnemy_ && lockOnManager_->IsLockedEnemyAlive()) {
				aimTarget = lockedEnemy_;
			} else if (aimAssistEnemy_) {
				aimTarget = aimAssistEnemy_;
			}
			missilePresetManager_->FirePlayerMissile(MissileType::Normal, aimTarget);
		}

		// 右クリチE���E��Eを引きながら敵へ曲がるホ�Eミング弾
		if (input->TriggerMouseButton(1)) {
			lockOnManager_->BeginMultiLock();
		}
		if (isMultiLockCharging_ && input->PushMouseButton(1)) {
			lockOnManager_->UpdateMultiLock(activeCamera);
		}
		if (isMultiLockCharging_ && !input->PushMouseButton(1)) {
			lockOnManager_->FireMultiLockMissiles();
		}
	} else if (isMultiLockCharging_) {
		lockOnManager_->CancelMultiLock();
	}

	// ==========================================
	// 弾の更新処琁E
	// ==========================================
	std::vector<Vector3> hitPositions;
	std::vector<Vector3> destroyedPositions;
	if (updateSelectedMissiles) {
		if (missileManager_) {
			missileManager_->Update(activeCamera, enemies_, obstacles_, hitPositions, destroyedPositions, lockedEnemy_);
		}

		if (explosionManager_ && !hitPositions.empty()) {
			explosionManager_->CreateHitEffects(hitPositions);
		}
		if (explosionManager_ && !destroyedPositions.empty()) {
			explosionManager_->CreateDestructionEffects(destroyedPositions);
		}

	} else {
		if (missileManager_) {
			missileManager_->UpdateModels(activeCamera);
		}
	}

	// 爁E��マネージャーの更新
	if ((!isSimulation || updateSelectedMissiles || updateSelectedParticles || (shouldUpdateGame && isFullFlowPreview)) && explosionManager_) {
		explosionManager_->Update();
	}

	// 大允E�Eパ�EチE��クル全体�E更新
	if (!isSimulation || updateSelectedMissiles || updateSelectedParticles || (shouldUpdateGame && isFullFlowPreview)) {
	}

	// ==========================================
	// チE��チE��用コライダー頂点構篁E
	// ==========================================
	if (showDebugColliders && updateDebugWireframes && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		std::vector<VertexData> colliderVertices;
		const bool drawAllDebugFrames = !isSelectedOnlyPreview || isFullFlowPreview;
		const bool drawPlayerDebugFrame = drawAllDebugFrames || uiManager_->currentSimulationTarget_ == 0;
		const bool drawMissileDebugFrame = drawAllDebugFrames || uiManager_->currentSimulationTarget_ == 1;
		const bool drawEnemyDebugFrame = drawAllDebugFrames || uiManager_->currentSimulationTarget_ == 2;
		const bool drawObstacleDebugFrame = drawAllDebugFrames;

		auto pushLine = [&](const Vector3& start, const Vector3& end, const Vector4& color) {
			VertexData v1{};
			VertexData v2{};
			v1.position = { start.x, start.y, start.z, 1.0f };
			v2.position = { end.x, end.y, end.z, 1.0f };
			v1.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
			v2.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
			v1.texcoord = { 0.0f, 0.0f, 0.0f, 0.0f };
			v2.texcoord = { 1.0f, 1.0f, 0.0f, 0.0f };
			v1.color = color;
			v2.color = color;
			colliderVertices.push_back(v1);
			colliderVertices.push_back(v2);
		};

		auto addAABB = [&](const Vector3& center, const Vector3& extents, const Vector4& color) {
			Vector3 p[8] = {
				{ center.x - extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z + extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z + extents.z },
			};

			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addOBB = [&](const Vector3& center, const Vector3& extents, const Vector3& rotation, const Vector4& color) {
			Vector3 local[8] = {
				{ -extents.x, -extents.y, -extents.z },
				{  extents.x, -extents.y, -extents.z },
				{  extents.x,  extents.y, -extents.z },
				{ -extents.x,  extents.y, -extents.z },
				{ -extents.x, -extents.y,  extents.z },
				{  extents.x, -extents.y,  extents.z },
				{  extents.x,  extents.y,  extents.z },
				{ -extents.x,  extents.y,  extents.z },
			};
			const Matrix4x4 rotationMatrix = MyMath::Multiply(
				MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation.x), MyMath::MakeRotateYMatrix(rotation.y)),
				MyMath::MakeRotateZMatrix(rotation.z));

			Vector3 p[8]{};
			for (int i = 0; i < 8; ++i) {
				const Vector3 rotated = MyMath::Transform(local[i], rotationMatrix);
				p[i] = AddVector3(center, rotated);
			}

			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addOBBShape = [&](const OBB& obb, const Vector4& color) {
			Vector3 p[8] = {
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
			};
			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addSphere = [&](const Vector3& center, float radius, const Vector4& color) {
			constexpr int segmentCount = 24;
			constexpr float twoPi = 6.283185307f;
			for (int i = 0; i < segmentCount; ++i) {
				const float angle1 = twoPi * static_cast<float>(i) / static_cast<float>(segmentCount);
				const float angle2 = twoPi * static_cast<float>(i + 1) / static_cast<float>(segmentCount);
				const float cos1 = std::cos(angle1);
				const float sin1 = std::sin(angle1);
				const float cos2 = std::cos(angle2);
				const float sin2 = std::sin(angle2);

				pushLine(
					{ center.x + radius * cos1, center.y + radius * sin1, center.z },
					{ center.x + radius * cos2, center.y + radius * sin2, center.z },
					color);
				pushLine(
					{ center.x, center.y + radius * cos1, center.z + radius * sin1 },
					{ center.x, center.y + radius * cos2, center.z + radius * sin2 },
					color);
				pushLine(
					{ center.x + radius * sin1, center.y, center.z + radius * cos1 },
					{ center.x + radius * sin2, center.y, center.z + radius * cos2 },
					color);
			}
		};

		// 1. プレイヤーのAABBと琁E
		if (drawPlayerDebugFrame && player_ && !player_->IsDead()) {
			addOBBShape(player_->GetOBB(), { 0.0f, 1.0f, 0.0f, 1.0f });
		}

		// 2. 障害物のAABB
		if (drawObstacleDebugFrame) {
			for (const auto& obstacle : obstacles_) {
				if (!obstacle || obstacle->IsStageBounds()) {
					continue;
				}
				// Model��の実際のバウンチE��ングボックス ÁEBlenderスケール = 正確なワールドAABB
				addOBBShape(obstacle->GetOBB(), { 0.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		// 3. 敵のAABBと琁E
		if (drawEnemyDebugFrame) {
			for (const auto& enemy : enemies_) {
				if (!enemy->IsDead()) {
					addOBBShape(enemy->GetOBB(), { 1.0f, 0.0f, 0.0f, 1.0f });
				}
			}
		}

		if ((drawEnemyDebugFrame || drawMissileDebugFrame) && lockedEnemy_ && !lockedEnemy_->IsDead()) {
			addSphere(lockedEnemy_->GetPosition(), lockedEnemy_->GetCollisionRadius() + 0.35f, { 1.0f, 0.95f, 0.0f, 1.0f });
		}

		// 4. 自機ミサイル�E�Elayer Bullets�E�E
		if (drawMissileDebugFrame && missileManager_) {
			for (const auto& missile : missileManager_->GetMissiles()) {
				if (!missile->IsDead()) {
					// Sphere: Magenta (radius: 0.5f)
					addSphere(missile->GetPosition(), missile->GetCollisionRadius(), { 1.0f, 0.0f, 1.0f, 1.0f });
				}
			}
		}

		// 5. 敵の弾�E�Enemy Bullets�E�E
		if (drawEnemyDebugFrame && enemyBulletManager_) {
			for (const auto& bullet : enemyBulletManager_->GetBullets()) {
				if (!bullet.isDead) {
					// Sphere: Orange (radius: 0.5f)
					addSphere(bullet.position, 0.5f, { 1.0f, 0.5f, 0.0f, 1.0f });
				}
			}
		}
		// 空の場合EダミEの透Eな線を追加Eリソース stuck 防止EE
		if (colliderVertices.empty()) {
			VertexData v1, v2;
			v1.position = { 0.0f, 0.0f, 0.0f, 1.0f };
			v1.color = { 0.0f, 0.0f, 0.0f, 0.0f };
			v2.position = { 0.0f, 0.0f, 0.0f, 1.0f };
			v2.color = { 0.0f, 0.0f, 0.0f, 0.0f };
			colliderVertices.push_back(v1);
			colliderVertices.push_back(v2);
		}

		debugColliderLinesObject->GetModel()->UpdateLineVertices(colliderVertices);
		debugColliderLinesObject->Update();
	}

	if (environmentRenderer_) {
		environmentRenderer_->Update(camera.get());
	}

#ifdef ENABLE_IMGUI
	if (uiManager_) {
		uiManager_->UpdateUI();
	}
#endif
	sprite->Update();
}

void GamePlayScene::Draw() {
	//3Dオブジェト描画準備
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// プレイヤーの描画
	if (player_) {
		player_->Draw();
	}

	bool isAnimationEditor = IsSimulationMode() && uiManager_ && uiManager_->currentSimulationTarget_ == 5;
	if (isAnimationEditor) {
		// アニメーション編集時は常にボーンを描画
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
		if (skeletonLinesObject && skeletonLinesObject->GetModel()) {
			skeletonLinesObject->Draw();
		}
		return; // アニメーション編集時はプレイヤーとボーンのみ描画
	}

	// すべてのミサイルを描画
	if (missileManager_) {
		missileManager_->Draw();
	}

	// 敵の弾を描画
	if (enemyBulletManager_) {
		enemyBulletManager_->Draw();
	}

	Vector4 frustumPlanes[6];
	MyMath::ExtractFrustumPlanes(camera->GetViewProjectionMatrix(), frustumPlanes);

	// 敵の描画
	for (const auto &enemy : enemies_) {
		Sphere enemySphere;
		enemySphere.center = enemy->GetPosition();
		enemySphere.radius = enemy->GetCollisionRadius();

		// 画面外の場合に描画しない（カリング）
		if (MyMath::IsInFrustum(enemySphere, frustumPlanes)) {
			enemy->Draw();
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}
	for (const AmmoPickup &pickup : ammoPickups_) {
		if (pickup.object) {
			pickup.object->Draw();
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}

	// 障害物の描画
	for (const auto &obstacle : obstacles_) {
		Sphere obsSphere;
		obsSphere.center = obstacle->GetPosition();
		obsSphere.radius = MyMath::Length(obstacle->GetWorldHalfExtents());

		// 画面外�E場合�E描画しなぁE��カリング�E�E
		if (MyMath::IsInFrustum(obsSphere, frustumPlanes)) {
			obstacle->Draw();
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	//3Dオブジェクト�E描画
	if (showPlane) {
		for (Object3d* object3d : objects) {
			object3d->Draw();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// アニメーションModel��の個別描画制御
	if (showModel && myModelObject) {
		myModelObject->Draw();
	}

	if (player_ && boundaryAlertObject_ && ceilingBoundaryAlertObject_) {
		static float pulseTime = 0.0f;
		pulseTime += 0.05f;
		float pulseAlpha = 0.5f + 0.5f * std::sin(pulseTime);

		auto drawBoundaryAlert = [&](Object3d* alertObject, const Vector3& position, const Vector3& normal, float intensity) {
			alertObject->SetScale({ 2.0f, 2.0f, 2.0f });

			// The plane model is already upright on the XY plane. Walls only need yaw;
			// the ceiling needs a pitch so the alert lies on the horizontal surface.
			Vector3 rotate = { 0.0f, std::atan2(normal.x, normal.z), 0.0f };
			if (normal.y > 0.5f) {
				rotate = { -1.570796f, 0.0f, 0.0f };
			}

			Model* m = alertObject->GetModel();
			if (m) {
				m->SetColor({ 1.0f, 1.0f, 1.0f, intensity * pulseAlpha });
			}

			alertObject->SetTranslate({
				position.x + normal.x * 0.5f,
				position.y + normal.y * 0.5f,
				position.z + normal.z * 0.5f
			});

			alertObject->SetRotate(rotate);
			alertObject->Update();
			alertObject->Draw();
		};

		if (player_->IsNearWallBoundary()) {
			drawBoundaryAlert(
				boundaryAlertObject_.get(),
				player_->GetWallBoundaryAlertPosition(),
				player_->GetWallBoundaryAlertNormal(),
				player_->GetWallBoundaryWarningIntensity());
		}
		if (player_->IsNearCeilingBoundary()) {
			drawBoundaryAlert(
				ceilingBoundaryAlertObject_.get(),
				player_->GetCeilingBoundaryAlertPosition(),
				player_->GetCeilingBoundaryAlertNormal(),
				player_->GetCeilingBoundaryWarningIntensity());
		}
		if (player_->IsNearBoundary()) {
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}
	
	if (showBones) {
		// ボ�Eン描画の前に設定を確実にする
		Object3dCommon::GetInstance()->SetCommonDrawSettings();

		// ボ�Eンラインの描画
		if (skeletonLinesObject && skeletonLinesObject->GetModel()) {
			skeletonLinesObject->Draw();
		}
	}

	if (showDebugColliders && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		debugColliderLinesObject->Draw();
	}
	
	// エフェクト系の描画 (深度書き込み無効)
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (environmentRenderer_) environmentRenderer_->Draw();

	// explosionManagerはObject3d(リング)を描画するため、再度設定を呼び出す
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (explosionManager_) explosionManager_->Draw();


	//Spriteの描画基溁E
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	//スプライト描画
	if (showSprite) {
		sprite->Draw();
	}

	DrawOverlay();
}

void GamePlayScene::DrawOverlay() {
	if (isDebugCameraActive_ && !debugFlyCamera_) return;
	if (!isDebugCameraActive_ && !camera) return;

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	if (!activeCamera) return;

	// 左下はHP/SP、右下は弾薬と役割ごとにHUDを分ける。
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	const float statusPanelX = 20.0f;
	const float statusPanelY = screenHeight - 145.0f;
	const float ammoPanelX = screenWidth - 420.0f;
	const float ammoPanelY = screenHeight - 170.0f;
	if (hudPanelSprite_) {
		hudPanelSprite_->SetPosition({ statusPanelX, statusPanelY });
		hudPanelSprite_->SetSize({ 390.0f, 125.0f });
		hudPanelSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.92f });
		hudPanelSprite_->Update();
		hudPanelSprite_->Draw();
	}
	if (hudAmmoPanelSprite_) {
		hudAmmoPanelSprite_->SetPosition({ ammoPanelX, ammoPanelY });
		hudAmmoPanelSprite_->SetSize({ 400.0f, 150.0f });
		hudAmmoPanelSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.92f });
		hudAmmoPanelSprite_->Update();
		hudAmmoPanelSprite_->Draw();
	}

	auto drawLabel = [](Sprite *label, const Vector2 &position, const Vector2 &size) {
		if (!label) return;
		label->SetPosition(position);
		label->SetSize(size);
		label->Update();
		label->Draw();
	};
	drawLabel(hudHpLabelSprite_.get(), { statusPanelX + 30.0f, statusPanelY + 29.0f }, { 42.0f, 25.0f });
	drawLabel(hudSpLabelSprite_.get(), { statusPanelX + 30.0f, statusPanelY + 77.0f }, { 39.0f, 25.0f });
	drawLabel(hudAmmoLabelSprite_.get(), { ammoPanelX + 32.0f, ammoPanelY + 22.0f }, { 82.0f, 25.0f });

	auto drawText = [](const std::string &text, auto &sprites, float rightX, float y, const Vector4 &color) {
		constexpr float digitWidth = 22.0f;
		constexpr float digitHeight = 28.0f;
		const float startX = rightX - digitWidth * static_cast<float>(text.size());
		for (size_t index = 0; index < text.size() && index < sprites.size(); ++index) {
			Sprite *digitSprite = sprites[index].get();
			const int glyphIndex = (text[index] == '/') ? 10 : text[index] - '0';
			digitSprite->SetTextureLeftTop({ static_cast<float>(glyphIndex * 64), 0.0f });
			digitSprite->SetTextureSize({ 64.0f, 80.0f });
			digitSprite->SetPosition({ startX + digitWidth * static_cast<float>(index), y });
			digitSprite->SetSize({ digitWidth, digitHeight });
			digitSprite->SetColor(color);
			digitSprite->Update();
			digitSprite->Draw();
		}
	};
	const float statusGaugeX = statusPanelX + 88.0f;
	const float statusGaugeWidth = 265.0f;
	const float statusGaugeHeight = 18.0f;
	if (hpGaugeBackgroundSprite_ && hpGaugeFillSprite_) {
		hpGaugeBackgroundSprite_->SetPosition({ statusGaugeX - 2.0f, statusPanelY + 32.0f });
		hpGaugeBackgroundSprite_->SetSize({ statusGaugeWidth + 4.0f, statusGaugeHeight + 4.0f });
		hpGaugeBackgroundSprite_->SetColor({ 0.04f, 0.03f, 0.08f, 0.92f });
		hpGaugeBackgroundSprite_->Update();
		hpGaugeBackgroundSprite_->Draw();
		const float hpRatio = std::clamp(
			static_cast<float>(player_ ? player_->GetHP() : 0) / static_cast<float>(Player::kMaxHP),
			0.0f,
			1.0f);
		hpGaugeFillSprite_->SetPosition({ statusGaugeX, statusPanelY + 34.0f });
		hpGaugeFillSprite_->SetSize({ statusGaugeWidth * hpRatio, statusGaugeHeight });
		hpGaugeFillSprite_->SetColor({ 1.0f, 0.12f, 0.18f, 1.0f });
		hpGaugeFillSprite_->Update();
		hpGaugeFillSprite_->Draw();
	}
	if (hudNormalAmmoIconSprite_ && hudHomingAmmoIconSprite_) {
		hudNormalAmmoIconSprite_->SetTextureLeftTop({ 0.0f, 0.0f });
		hudNormalAmmoIconSprite_->SetTextureSize({ 887.0f, 887.0f });
		hudNormalAmmoIconSprite_->SetPosition({ ammoPanelX + 34.0f, ammoPanelY + 53.0f });
		hudNormalAmmoIconSprite_->SetSize({ 48.0f, 48.0f });
		hudNormalAmmoIconSprite_->Update();
		hudNormalAmmoIconSprite_->Draw();
		hudHomingAmmoIconSprite_->SetTextureLeftTop({ 887.0f, 0.0f });
		hudHomingAmmoIconSprite_->SetTextureSize({ 887.0f, 887.0f });
		hudHomingAmmoIconSprite_->SetPosition({ ammoPanelX + 34.0f, ammoPanelY + 91.0f });
		hudHomingAmmoIconSprite_->SetSize({ 48.0f, 48.0f });
		hudHomingAmmoIconSprite_->Update();
		hudHomingAmmoIconSprite_->Draw();
	}
	const Vector4 normalAmmoColor = isNormalReloading_
		? Vector4{ 1.0f, 1.0f, 0.65f, 1.0f }
		: Vector4{ 1.0f, 0.78f, 0.08f, 1.0f };
	const Vector4 homingAmmoColor = isHomingReloading_
		? Vector4{ 1.0f, 0.65f, 0.65f, 1.0f }
		: Vector4{ 1.0f, 0.18f, 0.12f, 1.0f };
	drawText(std::to_string(normalAmmoInMagazine_) + "/" + std::to_string(normalAmmoReserve_), hudNormalAmmoDigitSprites_, ammoPanelX + 365.0f, ammoPanelY + 60.0f, normalAmmoColor);
	drawText(std::to_string(homingAmmoInMagazine_) + "/" + std::to_string(homingAmmoReserve_), hudHomingAmmoDigitSprites_, ammoPanelX + 365.0f, ammoPanelY + 98.0f, homingAmmoColor);
	if (isNormalReloading_ && hudNormalReloadGaugeSprite_) {
		hudNormalReloadGaugeSprite_->SetPosition({ ammoPanelX + 90.0f, ammoPanelY + 89.0f });
		hudNormalReloadGaugeSprite_->SetSize({ 275.0f * static_cast<float>(normalReloadFrame_) / static_cast<float>(kReloadDurationFrames), 4.0f });
		hudNormalReloadGaugeSprite_->SetColor({ 1.0f, 0.78f, 0.08f, 1.0f });
		hudNormalReloadGaugeSprite_->Update();
		hudNormalReloadGaugeSprite_->Draw();
	}
	if (isHomingReloading_ && hudHomingReloadGaugeSprite_) {
		hudHomingReloadGaugeSprite_->SetPosition({ ammoPanelX + 90.0f, ammoPanelY + 127.0f });
		hudHomingReloadGaugeSprite_->SetSize({ 275.0f * static_cast<float>(homingReloadFrame_) / static_cast<float>(kReloadDurationFrames), 4.0f });
		hudHomingReloadGaugeSprite_->SetColor({ 1.0f, 0.18f, 0.12f, 1.0f });
		hudHomingReloadGaugeSprite_->Update();
		hudHomingReloadGaugeSprite_->Draw();
	}

	// 中央の白線が必殺技1回分（50%）。
	const float gaugeX = statusGaugeX;
	const float gaugeY = statusPanelY + 82.0f;
	const float gaugeWidth = statusGaugeWidth;
	const float gaugeHeight = 18.0f;
	if (spGaugeBackgroundSprite_ && spGaugeFillSprite_ && spGaugeCostMarkerSprite_) {
		spGaugeBackgroundSprite_->SetPosition({ gaugeX - 2.0f, gaugeY - 2.0f });
		spGaugeBackgroundSprite_->SetSize({ gaugeWidth + 4.0f, gaugeHeight + 4.0f });
		spGaugeBackgroundSprite_->SetColor({ 0.03f, 0.05f, 0.12f, 0.90f });
		spGaugeBackgroundSprite_->Update();
		spGaugeBackgroundSprite_->Draw();

		spGaugeFillSprite_->SetPosition({ gaugeX, gaugeY });
		spGaugeFillSprite_->SetSize({ gaugeWidth * std::clamp(spGauge_ / 100.0f, 0.0f, 1.0f), gaugeHeight });
		spGaugeFillSprite_->SetColor(isSpecialAttackActive_
			? Vector4{ 1.0f, 0.35f, 0.05f, 1.0f }
			: Vector4{ 0.05f, 0.75f, 1.0f, 1.0f });
		spGaugeFillSprite_->Update();
		spGaugeFillSprite_->Draw();

		spGaugeCostMarkerSprite_->SetPosition({ gaugeX + gaugeWidth * 0.5f - 1.0f, gaugeY });
		spGaugeCostMarkerSprite_->SetSize({ 2.0f, gaugeHeight });
		spGaugeCostMarkerSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.9f });
		spGaugeCostMarkerSprite_->Update();
		spGaugeCostMarkerSprite_->Draw();
	}

	DrawRadar();

	bool isJammed = lockOnManager_->IsPlayerJammed(activeCamera);

	if (isMultiLockCharging_ && !multiLockTargets_.empty()) {
		for (Enemy *target : multiLockTargets_) {
			DrawLockOnOverlaySprite(target, activeCamera->GetViewProjectionMatrix(), lockOnReticleSprite_.get(), isJammed);
		}
	} else if (Enemy *overlayTarget = lockedEnemy_ ? lockedEnemy_ : aimAssistEnemy_) {
		DrawLockOnOverlaySprite(overlayTarget, activeCamera->GetViewProjectionMatrix(), lockOnReticleSprite_.get(), isJammed);
	} else {
		DrawAimCursorOverlaySprite(aimCursorSprite_.get(), isJammed);
	}
}

void GamePlayScene::DrawRadar() {
	if (!player_ || !radarFrameSprite_) {
		return;
	}

	constexpr float kRadarRange = 250.0f;
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	const float radarSize = (std::min)(230.0f, screenHeight * 0.30f);
	const float radarRadius = radarSize * 0.39f;
	const Vector2 radarCenter = {
		screenWidth - radarSize * 0.5f - 20.0f,
		radarSize * 0.5f + 20.0f
	};

	radarFrameSprite_->SetPosition(radarCenter);
	radarFrameSprite_->SetSize({ radarSize, radarSize });
	radarFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.96f });
	radarFrameSprite_->Update();
	radarFrameSprite_->Draw();

	// 中心から伸びる細い走査線を回転させる。
	radarSweepAngle_ += 0.025f;
	if (radarSweepAngle_ >= 6.2831853f) {
		radarSweepAngle_ -= 6.2831853f;
	}
	if (radarSweepSprite_) {
		radarSweepSprite_->SetPosition(radarCenter);
		radarSweepSprite_->SetSize({ radarRadius, 2.0f });
		radarSweepSprite_->SetRotation(radarSweepAngle_ - 1.5707963f);
		radarSweepSprite_->SetColor({ 0.0f, 0.9f, 1.0f, 0.48f });
		radarSweepSprite_->Update();
		radarSweepSprite_->Draw();
	}

	const Vector3 playerPosition = player_->GetPosition();
	const Vector3 playerForward = player_->GetForwardVector();
	const float forwardLength = std::sqrt(
		playerForward.x * playerForward.x + playerForward.z * playerForward.z);
	const float forwardX = forwardLength > 0.0001f ? playerForward.x / forwardLength : 0.0f;
	const float forwardZ = forwardLength > 0.0001f ? playerForward.z / forwardLength : 1.0f;
	const float rightX = forwardZ;
	const float rightZ = -forwardX;

	size_t blipIndex = 0;
	for (const auto &enemy : enemies_) {
		if (!enemy || enemy->IsDead() || blipIndex >= radarBlipSprites_.size()) {
			continue;
		}

		const Vector3 enemyPosition = enemy->GetPosition();
		const float deltaX = enemyPosition.x - playerPosition.x;
		const float deltaZ = enemyPosition.z - playerPosition.z;
		const float localRight = deltaX * rightX + deltaZ * rightZ;
		const float localForward = deltaX * forwardX + deltaZ * forwardZ;
		const float distance = std::sqrt(localRight * localRight + localForward * localForward);
		const float positionScale = distance > kRadarRange && distance > 0.0001f
			? kRadarRange / distance
			: 1.0f;

		Sprite *blip = radarBlipSprites_[blipIndex++].get();
		blip->SetPosition({
			radarCenter.x + (localRight * positionScale / kRadarRange) * radarRadius,
			radarCenter.y - (localForward * positionScale / kRadarRange) * radarRadius
		});
		const float blipSize = enemy->IsBoss() ? 12.0f : (enemy.get() == lockedEnemy_ ? 9.0f : 6.0f);
		blip->SetSize({ blipSize, blipSize });
		if (enemy.get() == lockedEnemy_) {
			blip->SetColor({ 1.0f, 0.95f, 0.05f, 1.0f });
		} else if (enemy->IsBoss()) {
			blip->SetColor({ 1.0f, 0.12f, 0.05f, 1.0f });
		} else {
			blip->SetColor({ 1.0f, 0.38f, 0.05f, 1.0f });
		}
		blip->Update();
		blip->Draw();
	}
}


