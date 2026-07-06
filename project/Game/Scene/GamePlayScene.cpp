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
}



#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
	bool gShowStageValidationWindow = true;
	bool gShowStageValidationLabels = true;

	ImVec4 ValidationLevelColor(const std::string &level) {
		if (level == "ERROR") {
			return ImVec4(1.0f, 0.25f, 0.2f, 1.0f);
		}
		if (level == "WARNING") {
			return ImVec4(1.0f, 0.75f, 0.2f, 1.0f);
		}
		return ImVec4(0.35f, 1.0f, 0.45f, 1.0f);
	}

	const char *ValidationLevelText(const std::string &level) {
		if (level == "ERROR") {
			return "ERROR";
		}
		if (level == "WARNING") {
			return "WARNING";
		}
		return "OK";
	}

	void DrawValidationMessages(const char *title, const std::vector<std::string> &messages, const ImVec4 &color) {
		ImGui::TextColored(color, "%s: %d", title, static_cast<int>(messages.size()));
		for (const std::string &message : messages) {
			ImGui::BulletText("%s", message.c_str());
		}
	}

	void DrawStageValidationWindow(const StageValidation::Report &report) {
		ImGui::Begin("Level Validation");
		ImGui::Text("Source: %s", report.source.c_str());

		DrawValidationMessages("Errors", report.errors, ValidationLevelColor("ERROR"));
		DrawValidationMessages("Warnings", report.warnings, ValidationLevelColor("WARNING"));

		ImGui::Separator();
		ImGui::Text("Check Items");
		for (const StageValidation::CheckItemResult &item : report.checkItems) {
			ImGui::TextColored(ValidationLevelColor(item.level), "%s", ValidationLevelText(item.level));
			ImGui::SameLine(90.0f);
			if (item.detail.empty()) {
				ImGui::TextWrapped("%s", item.label.c_str());
			} else {
				ImGui::TextWrapped("%s: %s", item.label.c_str(), item.detail.c_str());
			}
		}

		ImGui::End();
	}

	struct ValidationLabelRect {
		ImVec2 min;
		ImVec2 max;
	};

	float ClampValidationFloat(float value, float minValue, float maxValue) {
		if (maxValue < minValue) {
			return minValue;
		}
		if (value < minValue) {
			return minValue;
		}
		if (value > maxValue) {
			return maxValue;
		}
		return value;
	}

	ValidationLabelRect MakeValidationLabelRect(const ImVec2 &textPos, const ImVec2 &textSize, float padding) {
		return {
			ImVec2(textPos.x - padding, textPos.y - padding),
			ImVec2(textPos.x + textSize.x + padding, textPos.y + textSize.y + padding),
		};
	}

	bool ValidationRectsOverlap(const ValidationLabelRect &lhs, const ValidationLabelRect &rhs) {
		return lhs.min.x < rhs.max.x && lhs.max.x > rhs.min.x &&
			lhs.min.y < rhs.max.y && lhs.max.y > rhs.min.y;
	}

	bool ValidationRectOverlapsAny(const ValidationLabelRect &rect, const std::vector<ValidationLabelRect> &usedRects) {
		for (const ValidationLabelRect &usedRect : usedRects) {
			if (ValidationRectsOverlap(rect, usedRect)) {
				return true;
			}
		}
		return false;
	}

	void ClampValidationLabelToGameView(
		ImVec2 &textPos,
		const ImVec2 &textSize,
		float padding,
		float minX,
		float minY,
		float maxX,
		float maxY) {

		textPos.x = ClampValidationFloat(textPos.x, minX + padding, maxX - textSize.x - padding);
		textPos.y = ClampValidationFloat(textPos.y, minY + padding, maxY - textSize.y - padding);
	}

	ImVec2 ResolveValidationLabelPosition(
		const ImVec2 &anchor,
		const ImVec2 &textSize,
		float padding,
		float minX,
		float minY,
		float maxX,
		float maxY,
		std::vector<ValidationLabelRect> &usedRects) {

		const float rectWidth = textSize.x + padding * 2.0f;
		const float rectHeight = textSize.y + padding * 2.0f;
		const float verticalStep = rectHeight + 4.0f;
		const float horizontalStep = rectWidth * 0.55f;
		const int horizontalSlots[] = { 0, -1, 1, -2, 2 };

		for (int stackIndex = 0; stackIndex < 10; ++stackIndex) {
			for (int side = 0; side < 2; ++side) {
				for (int horizontalSlot : horizontalSlots) {
					const float x = anchor.x - textSize.x * 0.5f + horizontalStep * static_cast<float>(horizontalSlot);
					const float y = (side == 0)
						? anchor.y - textSize.y - 22.0f - verticalStep * static_cast<float>(stackIndex)
						: anchor.y + 16.0f + verticalStep * static_cast<float>(stackIndex);
					ImVec2 candidate(x, y);
					ClampValidationLabelToGameView(candidate, textSize, padding, minX, minY, maxX, maxY);

					const ValidationLabelRect rect = MakeValidationLabelRect(candidate, textSize, padding);
					if (!ValidationRectOverlapsAny(rect, usedRects)) {
						usedRects.push_back(rect);
						return candidate;
					}
				}
			}
		}

		ImVec2 fallback(anchor.x - textSize.x * 0.5f, anchor.y - textSize.y - 22.0f);
		ClampValidationLabelToGameView(fallback, textSize, padding, minX, minY, maxX, maxY);
		usedRects.push_back(MakeValidationLabelRect(fallback, textSize, padding));
		return fallback;
	}

	void DrawStageValidationOverlay(const StageValidation::Report &report, const Matrix4x4 &viewProjectionMatrix) {
		if (!report.HasMarkers()) {
			return;
		}

		float minX = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float maxY = 0.0f;
		if (!FlyCamera::GetGameViewBounds(minX, minY, maxX, maxY)) {
			return;
		}

		const float width = maxX - minX;
		const float height = maxY - minY;
		if (!ImGui::GetCurrentContext()) {
			return;
		}
		ImDrawList *drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
		std::vector<ValidationLabelRect> usedRects;

		for (const StageValidation::ValidationMarker &marker : report.markers) {
			Vector3 worldPosition = {
				static_cast<float>(marker.x),
				static_cast<float>(marker.y) + 1.2f,
				static_cast<float>(marker.z),
			};
			Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
			if (screenPosition.z < 0.0f || screenPosition.z > 1.0f) {
				continue;
			}
			if (screenPosition.x < 0.0f || screenPosition.x > width || screenPosition.y < 0.0f || screenPosition.y > height) {
				continue;
			}

			const bool isError = marker.level == "ERROR";
			const std::string text = std::string(isError ? "ERROR: " : "WARNING: ") + marker.message;
			const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
			const float padding = 4.0f;
			const ImVec2 anchor(minX + screenPosition.x, minY + screenPosition.y);
			const ImVec2 textPos = ResolveValidationLabelPosition(anchor, textSize, padding, minX, minY, maxX, maxY, usedRects);
			const ValidationLabelRect rect = MakeValidationLabelRect(textPos, textSize, padding);

			const ImU32 fillColor = isError ? IM_COL32(90, 16, 16, 220) : IM_COL32(92, 63, 0, 220);
			const ImU32 borderColor = isError ? IM_COL32(255, 75, 55, 255) : IM_COL32(255, 200, 45, 255);
			drawList->AddRectFilled(rect.min, rect.max, fillColor, 3.0f);
			drawList->AddRect(rect.min, rect.max, borderColor, 3.0f);
			drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text.c_str());
		}
	}
#endif

GamePlayScene::GamePlayScene(Mode mode)
	: mode_(mode) {
}


void GamePlayScene::Initialize() {

	//カメラ・シーンリソース
	camera = std::make_unique<Camera>();
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
	SkyboxCommon::GetInstance()->Initialize(DirectXCommon::GetInstance());

	// スカイボックスの生Eと初期匁E
	skybox = std::make_unique<Skybox>();
	skybox->Initialize("resources/SkyBox.dds");


	//Model��・パ�EチE��クル
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("multiMesh.obj");
	ModelManager::GetInstance()->CreateSphereModel("Sphere", 16);

	//======================================================
	// プリミティブE生EEE
	//======================================================

	// 地面のモデル
	groundModel = std::make_unique<Object3d>();
	groundModel->Initialize(Object3dCommon::GetInstance());
	groundModel->SetModel("plane.obj");
	groundModel->SetScale({ 100.0f, 1.0f, 100.0f });
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
	myRing = std::make_unique<Primitive>();
	myRing->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Ring);
	myRing->SetTranslate({ 0.0f, 0.0f, 0.0f }); // パ�EチE��クルと同じ中忁E��置に合わせる

	// 部刁E��ング (三日朁E
	myPartialRing = std::make_unique<Primitive>();
	myPartialRing->Initialize(Object3dCommon::GetInstance(), PrimitiveType::PartialRing);
	myPartialRing->SetTranslate({ 0.0f, 0.0f, 0.0f });

	// 冁E��エフェクチE
	myCylinder = std::make_unique<Primitive>();
	myCylinder->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Cylinder);
	myCylinder->SetTranslate({ 0.0f, 0.0f, 0.0f });
	myCylinder->SetScale({ 2.0f, 2.0f, 2.0f });

	//パ�EチE��クル
	particleManager = std::make_unique<ParticleManager>();
	particleManager->Initialize(DirectXCommon::GetInstance());
	particleManager->CreateParticleGroup("test", "resources/circle.png");
	particleEmitter = std::make_unique<ParticleEmitter>("test", Vector3{ 0.0f,0.0f,0.0f }, particleManager.get());

	//音声再生
	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");

	pVoice1=AudioManager::GetInstance()->PlayWave(soundData1, true);
	pVoice2=AudioManager::GetInstance()->PlayWave(soundData2, true);

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
	ModelManager::GetInstance()->CreateBoxModel("ObstacleBox");

	player_ = std::make_unique<Player>();
	player_->Initialize(kPlayerModelName);

	// 弾
	missileManager_ = std::make_unique<MissileManager>();
	missileManager_->Initialize();

	// 爁E��エフェクチE
	explosionManager_ = std::make_unique<ExplosionManager>();
	explosionManager_->Initialize(particleManager.get());

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
		currentSimulationTarget_ = 2;
		showSimulationWindow_ = true;
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
		explosionManager_->Initialize(particleManager.get());
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
		const EnemySpawnData &spawnData = enemySpawns_[spawnPointIndex];
		if (spawnData.reinforcementTriggerName == deadEnemyName) {
			ScheduleEnemySpawn(spawnPointIndex, spawnData.reinforcementDelayFrames);
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


























void GamePlayScene::DrawGameplayActionControls() {
#ifdef ENABLE_IMGUI
	ImGui::Separator();
	ImGui::Text("保存済みシミュレーション設定");
	ImGui::TextWrapped("シミュレーション画面で保存した内容を、現在のゲーム側の設定値として読み込みます。");
	if (ImGui::Button("保存一覧を更新")) {
		simulationManager_->RefreshSimulationActionNames();
	}

	if (simulationActionNames_.empty()) {
		ImGui::TextDisabled("保存されたシミュレーション設定がありません。");
		if (!simulationActionMessage_.empty()) {
			ImGui::TextWrapped("%s", simulationActionMessage_.c_str());
		}
		return;
	}

	std::vector<const char *> actionNameItems;
	actionNameItems.reserve(simulationActionNames_.size());
	for (const std::string &name : simulationActionNames_) {
		actionNameItems.push_back(name.c_str());
	}

	if (selectedSimulationActionIndex_ >= static_cast<int>(actionNameItems.size())) {
		selectedSimulationActionIndex_ = 0;
	}

	ImGui::Combo("読み込む設定", &selectedSimulationActionIndex_, actionNameItems.data(), static_cast<int>(actionNameItems.size()));
	if (ImGui::Button("この設定をゲームに読み込む")) {
		simulationManager_->ApplySimulationAction(kSimulationActionsFilePath, simulationActionNames_[selectedSimulationActionIndex_]);
	}

	if (!simulationActionMessage_.empty()) {
		ImGui::TextWrapped("%s", simulationActionMessage_.c_str());
	}
#endif
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
		pVoice2->DestroyVoice();
	}

	AudioManager::GetInstance()->UnloadWave(soundData1);
	AudioManager::GetInstance()->UnloadWave(soundData2);

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
	// ゲームオーバ�E判定と演�E進衁E
	// ==========================================
	if (!IsSimulationMode() && !isGameOver_ && player_ && player_->IsDead()) {
		isGameOver_ = true;
		gameOverTimer_ = 0;

		std::vector<Vector3> playerHitPos = { player_->GetPosition() };
		if (explosionManager_) {
			explosionManager_->CreateExplosions(playerHitPos);
		}

		if (pVoice2) {
			pVoice2->Stop();
		}
	}

	bool shouldUpdateGame = true;

	if (isGameOver_) {
		gameOverTimer_++;

		// 絶望�E白黒化�E�グレースケール�E�エフェクトを適用�E�E
		if (PostEffect::GetInstance()) {
			float effectProgress = static_cast<float>(gameOverTimer_) / 120.0f;
			if (effectProgress > 1.0f) {
				effectProgress = 1.0f;
			}
			float vignetteRadius = 0.62f - 0.22f * effectProgress;
			float blurIntensity = 1.5f + 3.0f * effectProgress;
			PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.38f, blurIntensity);
		}

		// 5フレームに1回だけ更新することで、スローモーション�E�世界停止�E�を実現�E�E
		shouldUpdateGame = (gameOverTimer_ % 5 == 0);

		// 紁E秒！E20フレーム�E�経過したら、正式にゲームオーバ�Eシーンへ遷移する�E�E
		if (gameOverTimer_ >= 120) {
			SceneManager::GetInstance()->ChangeScene("GAMEOVER");
		}
	} else {
		// 通常時�Eノ�EマルエフェクチE
		if (PostEffect::GetInstance()) {
			PostEffect::GetInstance()->SetEffectType(0); // 0: Normal
		}
	}
	shouldUpdateGame = shouldUpdateGame && isEditorPreviewPlaying_;
	const bool isSimulation = IsSimulationMode();
	const bool isFullFlowPreview = !isSimulation || simulationPlaybackMode_ == 1;
	const bool isSelectedOnlyPreview = isSimulation && simulationPlaybackMode_ == 0;
	const bool updateSelectedPlayer = shouldUpdateGame && canUseKeyboardInput && (isFullFlowPreview || currentSimulationTarget_ == 0);
	const bool updateSelectedMissiles = shouldUpdateGame && (isFullFlowPreview || currentSimulationTarget_ == 1);
	const bool updateSelectedEnemies = shouldUpdateGame && (isFullFlowPreview || currentSimulationTarget_ == 2);
	const bool updateSelectedParticles = shouldUpdateGame && (isFullFlowPreview || currentSimulationTarget_ == 3);
	const bool allowMouseMissileFire = shouldUpdateGame && canUseMouseInput && (!isSimulation || isFullFlowPreview);
	const bool allowLockOnBehavior = !isGameOver_ && (isFullFlowPreview || currentSimulationTarget_ == 1 || currentSimulationTarget_ == 2);
	const bool updateDebugWireframes = !isSimulation || isFullFlowPreview || updateSelectedPlayer || updateSelectedMissiles || updateSelectedEnemies || updateSelectedParticles;
	const bool updateAnimationPreview = !isSimulation || isFullFlowPreview;

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

		// 今�EめE��に合わせた状態で使ぁE��め、Skeletonから計算結果を取り�EしてBox/Modelに適用する
		if (!skeleton.joints.empty()) {
			myBox->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
			myBox->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
			myBox->SetScale(skeleton.joints[skeleton.root].transform.scale);

			// スキニングが実裁E��れたため、スキンなしModel��の場合�EみTransformを適用する
			// スキニングが実裁E��れたため、スキンなしModel��の場合�EみTransformを適用する
			if (myModelObject->GetModel()) {
				if (!myModelObject->skinCluster.isValid) {
					myModelObject->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
					myModelObject->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
					myModelObject->SetScale(skeleton.joints[skeleton.root].transform.scale);
				} else {
					// スキニングModel��はアニメーションが行�Eに含まれるため、�EースのトランスフォームはリセチE��する
					// (これを行わなぁE��二重に移動して画面外に消えめE
					myModelObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
					myModelObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
					myModelObject->SetScale({ modelScale, modelScale, modelScale });
				}
			}
		}

		// 骨描画の更新
		if (showBones && updateAnimationPreview) {
			std::vector<VertexData> lineVertices;

			for (size_t i = 0; i < skeleton.joints.size(); ++i) {
				Vector3 pos = {
					skeleton.joints[i].skeletonSpaceMatrix.m[3][0],
					skeleton.joints[i].skeletonSpaceMatrix.m[3][1],
					skeleton.joints[i].skeletonSpaceMatrix.m[3][2]
				};
				// Model��全体�Eスケールに合わせてボ�Eンの座標もスケーリングする
				pos.x *= modelScale;
				pos.y *= modelScale;
				pos.z *= modelScale;

				// ライン用の頂点を作�E�E�親がいる場合！E
				if (skeleton.joints[i].parent) {
					int32_t parentIndex = *skeleton.joints[i].parent;
					Vector3 parentPos = {
						skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][0],
						skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][1],
						skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][2]
					};
					parentPos.x *= modelScale;
					parentPos.y *= modelScale;
					parentPos.z *= modelScale;

					VertexData v1, v2;
					v1.position = { parentPos.x, parentPos.y, parentPos.z, 1.0f };
					v1.normal = { 0.0f, 1.0f, 0.0f };
					v1.texcoord = { 0.0f, 0.0f };
					v1.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色

					v2.position = { pos.x, pos.y, pos.z, 1.0f };
					v2.normal = { 0.0f, 1.0f, 0.0f };
					v2.texcoord = { 1.0f, 1.0f };
					v2.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色

					lineVertices.push_back(v1);
					lineVertices.push_back(v2);
				}
			}

			// ラインModel��の頂点を更新
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

	// プレイヤーの更新と、カメラの追征E
	if (player_) {
		if (updateSelectedPlayer) {
			if (lockedEnemy_) {
				player_->UpdateLockOnRotation(lockedEnemy_->GetPosition());
			}
			player_->Update(obstacles_);
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
			explosionManager_->CreateExplosions(enemyBulletHits);
		}

		for (auto it = enemies_.begin(); it != enemies_.end(); ) {
			(*it)->Update(playerPos, enemyBulletManager_.get(), obstacles_);
			if ((*it)->IsDead()) {
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
				it = enemies_.erase(it); // 当たった敵はリストから消滁E
			} else {
				++it;
			}
		}
		UpdateEnemyRespawns();

		if (!IsSimulationMode() && !isGameOver_ && enemies_.empty() && !HasPendingEnemySpawns()) {
			SceneManager::GetInstance()->ChangeScene("CLEAR");
			return;
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
		if (canUseKeyboardInput && canUseMouseInput) {
			debugFlyCamera_->Update(); // FlyCameraが入力の消化して自動更新する
		} else {
			debugFlyCamera_->Camera::Update();
		}
		skybox->Update(debugFlyCamera_.get());
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
		skybox->Update(camera.get());
	}

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
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

	if (myRing && showNormalRing) {
		static float ringTime = 0.0f;
		ringTime += 0.05f;

		// オブジェクト�E体�E回転めE��縮は行わず、UVスクロールのみでエフェクトを表現する
		// パ�EチE��クルのエフェクトを囲むようにスケールを調整
		myRing->SetRotate({ 0.0f, 0.0f, 0.0f });
		myRing->SetScale({ 2.0f, 2.0f, 1.0f });

		// UVスクロールとスケーリング
		Model* ringModel = myRing->GetModel();
		if (ringModel) {
			Vector3 uvScale = { 10.0f, 1.0f, 1.0f }; // U方向にScaleして細かい模様にする
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			// 賁E��の持E��通り、U方向！E成�E�E�を時間でスクロールさせて冁E��回転させめE
			Vector3 uvTranslate = { ringTime * 0.1f, 0.0f, 0.0f }; 
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			ringModel->SetUvTransform(uvTransform);
		}
		myRing->Update();
	}

	if (myPartialRing && showPartialRing) {
		static float pRingTime = 0.0f;
		pRingTime += 0.05f;

		// 部刁E��ングはV方向をスクロールさせたり、Z軸回転させたりしてアニメーションできる
		myPartialRing->SetRotate({ 0.0f, 0.0f, pRingTime * -0.5f }); // Z回転で三日月を回す
		myPartialRing->SetScale({ 2.0f, 2.0f, 1.0f });

		Model* pRingModel = myPartialRing->GetModel();
		if (pRingModel) {
			Vector3 uvScale = { 1.0f, 10.0f, 1.0f };
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			Vector3 uvTranslate = { 0.0f, pRingTime * 0.1f, 0.0f }; // V方向へスクロール
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			pRingModel->SetUvTransform(uvTransform);
		}
		myPartialRing->Update();
	}

	if (myCylinder && showCylinder) {
		cylinderUVOffset[0] += cylinderUVScrollSpeed[0];
		cylinderUVOffset[1] += cylinderUVScrollSpeed[1];

		Model* cModel = myCylinder->GetModel();
		if (cModel) {
			Vector3 uvScale = { 1.0f, 1.0f, 1.0f };
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			Vector3 uvTranslate = { cylinderUVOffset[0], cylinderUVOffset[1], 0.0f };
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			cModel->SetUvTransform(uvTransform);
			cModel->SetAlphaReference(cylinderAlphaReference);
		}
		
		// ImGuiの変数をCylinderに適用
		myCylinder->SetTranslate({ cylinderPos[0], cylinderPos[1], cylinderPos[2] });
		myCylinder->SetScale({ cylinderScale[0], cylinderScale[1], cylinderScale[2] });

		myCylinder->Update();
	}

	for (Object3d *object3d : objects) {
		object3d->Update();
	}

	Vector2 size = sprite->GetSize();
	size.x = 300.0f;
	size.y = 300.0f;
	sprite->SetSize(size);

	lockOnManager_->UpdateLockOn(activeCamera, allowLockOnBehavior);

	if (showParticles && updateSelectedParticles) {
		particleEmitter->Update();
	}


	// ==========================================
	// ミサイルの発封E�E琁E
	// ==========================================
	if (allowMouseMissileFire && player_ && !isGameOver_) {
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
	if (updateSelectedMissiles) {
		if (missileManager_) {
			missileManager_->Update(activeCamera, enemies_, obstacles_, hitPositions, lockedEnemy_);
		}

		// 爁E��マネージャーに座標リストを渡して、発生を依頼するだけ！E
		if (explosionManager_ && !hitPositions.empty()) {
			explosionManager_->CreateExplosions(hitPositions);
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
		particleManager->Update(activeCamera);
	}

	// ==========================================
	// チE��チE��用コライダー頂点構篁E
	// ==========================================
	if (showDebugColliders && updateDebugWireframes && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		std::vector<VertexData> colliderVertices;
		const bool drawAllDebugFrames = !isSelectedOnlyPreview || isFullFlowPreview;
		const bool drawPlayerDebugFrame = drawAllDebugFrames || currentSimulationTarget_ == 0;
		const bool drawMissileDebugFrame = drawAllDebugFrames || currentSimulationTarget_ == 1;
		const bool drawEnemyDebugFrame = drawAllDebugFrames || currentSimulationTarget_ == 2;
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

		// 空の場合�Eダミ�Eの透�Eな線を追加�E�リソース stuck 防止�E�E
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

#ifdef ENABLE_IMGUI
	UpdateUI();
#endif
	sprite->Update();
}

void GamePlayScene::Draw() {
	//3Dオブジェト�E描画準備
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// プレイヤーの描画
	if (player_) {
		player_->Draw();
	}

	// すべてのミサイルを描画
	if (missileManager_) {
		missileManager_->Draw();
	}

	// 敵の弾を描画
	if (enemyBulletManager_) {
		enemyBulletManager_->Draw();
	}

	// 敵の描画
	for (const auto &enemy : enemies_) {
		enemy->Draw();
	}

	Vector4 frustumPlanes[6];
	MyMath::ExtractFrustumPlanes(camera->GetViewProjectionMatrix(), frustumPlanes);

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

	if (showSkybox && skybox) {
		skybox->Draw();
	}
	
	// エフェクト系の描画 (深度書き込み無効)
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (myRing && showNormalRing) myRing->Draw();
	if (myPartialRing && showPartialRing) myPartialRing->Draw();
	if (myCylinder && showCylinder) myCylinder->Draw();
	if (explosionManager_) explosionManager_->Draw();


	//Spriteの描画基溁E
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	//スプライト描画
	if (showSprite) {
		sprite->Draw();
	}

	DrawOverlay();

	particleManager->Draw();
}

void GamePlayScene::DrawOverlay() {
	if (isDebugCameraActive_ && !debugFlyCamera_) return;
	if (!isDebugCameraActive_ && !camera) return;

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	if (!activeCamera) return;

	if (isMultiLockCharging_ && !multiLockTargets_.empty()) {
		for (Enemy *target : multiLockTargets_) {
			DrawLockOnOverlaySprite(target, activeCamera->GetViewProjectionMatrix(), lockOnReticleSprite_.get());
		}
	} else if (Enemy *overlayTarget = lockedEnemy_ ? lockedEnemy_ : aimAssistEnemy_) {
		DrawLockOnOverlaySprite(overlayTarget, activeCamera->GetViewProjectionMatrix(), lockOnReticleSprite_.get());
	} else {
		DrawAimCursorOverlaySprite(aimCursorSprite_.get());
	}
}






void GamePlayScene::UpdateUI() {
#ifdef ENABLE_IMGUI
	if (ImGuiManager::IsVisible()) {
		if (IsSimulationMode()) {
			simulationManager_->DrawSimulationScreenUI();
			return;
		}

		ImGui::Begin("Simulation");
		ImGui::Text("シミュレーション設定");
		ImGui::TextWrapped("保存済み設定の読み込みはここで行えます。細かい保存や確認は専用画面を開いてください。");
		if (ImGui::Button("シミュレーション画面を開く (F2)")) {
			LaunchSimulationExecutable();
		}
		DrawGameplayActionControls();
		ImGui::End();
		if (false) {

		// --- シミュレーション起動用ミニウィンドウ ---
		ImGui::Begin("シミュレーション", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
		if (ImGui::Button(showSimulationWindow_ ? "シミュレーションツールを閉じる" : "シミュレーションツールを開く")) {
			showSimulationWindow_ = !showSimulationWindow_;
		}
		ImGui::End();

		// --- シミュレーション詳細ウィンドウ ---
		if (showSimulationWindow_) {
			ImGui::SetNextWindowSize(ImVec2(600.0f, 500.0f), ImGuiCond_Once);
			ImGui::Begin("シミュレーションツール", &showSimulationWindow_);
			
			const char* categories[] = { "プレイヤー", "ミサイル", "敵 & イベント", "パーティクル", "カメラ" };
			ImGui::Combo("カテゴリ", &currentSimulationTarget_, categories, IM_ARRAYSIZE(categories));
			ImGui::Separator();

			if (currentSimulationTarget_ == 0) {
				ImGui::Text("プレイヤー移動設定");
				if (player_) {
					auto mode = player_->GetCurrentMode();
					const char* modeName = (mode == PlayerMode::Fighter) ? "ファイター (1キー)" : 
										   (mode == PlayerMode::Gerwalk) ? "ガウォーク (2キー)" : "バトロイド (3キー)";
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "現在の形態: %s", modeName);
					
					PlayerModeParams& p = player_->GetModeParams(mode);
					ImGui::SliderFloat("最大移動速度", &p.maxMoveSpeed, 0.01f, 1.0f);
					ImGui::SliderFloat("移動加速度", &p.moveAcceleration, 0.001f, 0.1f);
					ImGui::SliderFloat("移動減衰", &p.moveDamping, 0.1f, 1.0f);
					ImGui::SliderFloat("ピッチ回転速度", &p.pitchSpeed, 0.001f, 0.1f);
					ImGui::SliderFloat("ヨー回転速度", &p.yawSpeed, 0.001f, 0.1f);
					ImGui::SliderFloat("ロール回転速度", &p.rollSpeed, 0.001f, 0.1f);
				} else {
					ImGui::Text("プレイヤーが初期化されていません。");
				}
			}
			else if (currentSimulationTarget_ == 1) {
				missilePresetManager_->DrawMissileSettingsUI();
			}
			else if (currentSimulationTarget_ == 2) {
				ImGui::Text("=== 敵の出現とルート ===");
				ImGui::Text("Lock-on: %s", lockedEnemy_ ? "LOCKED" : "NONE");
				ImGui::Text("Tab: lock target / X: unlock");
				ImGui::DragFloat3("出現座標 (X,Y,Z)", newEnemyPos, 1.0f);

				if (ImGui::Button("敵を生成する！")) {
					auto newEnemy = std::make_unique<Enemy>();
					newEnemy->Initialize({ newEnemyPos[0], newEnemyPos[1], newEnemyPos[2] });
					enemies_.push_back(std::move(newEnemy));
				}

				ImGui::Separator();
				ImGui::Text("=== 敵のリスト (総数: %d) ===", (int)enemies_.size());
				int index = 0;
				for (const auto& enemy : enemies_) {
					Vector3 pos = enemy->GetPosition();
					ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
					index++;
				}

				ImGui::Separator();
				ImGui::Text("イベントツール（増援設定）");
				static int selectedTrigger = 0;
				static int selectedTarget = 0;
				static int eventDelay = 60;
				std::vector<const char*> enemyNames;
				for (const auto& sp : enemySpawns_) {
					enemyNames.push_back(sp.name.c_str());
				}
				if (!enemyNames.empty()) {
					if (selectedTrigger >= enemyNames.size()) selectedTrigger = 0;
					if (selectedTarget >= enemyNames.size()) selectedTarget = 0;
					ImGui::Combo("トリガーとなる敵", &selectedTrigger, enemyNames.data(), static_cast<int>(enemyNames.size()));
					ImGui::Combo("出現する敵(ターゲット)", &selectedTarget, enemyNames.data(), static_cast<int>(enemyNames.size()));
					ImGui::DragInt("出現までのディレイ(フレーム)", &eventDelay, 1, 0, 600);
					if (ImGui::Button("イベントを追加")) {
						enemyEventManager_.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
						enemySpawns_[selectedTarget].isInitialSpawn = false;
					}
					ImGui::SameLine();
					if (ImGui::Button("イベントを保存")) {
						enemyEventManager_.SaveEvents("resources/enemy_events.json");
					}
					for (size_t i = 0; i < enemyEventManager_.GetEvents().size(); ++i) {
						const auto& ev = enemyEventManager_.GetEvents()[i];
						ImGui::Text("[%d] %s が死んだら %d F後に %s が出現", (int)i, ev.triggerEnemyName.c_str(), ev.delayFrames, ev.targetEnemyName.c_str());
						ImGui::SameLine();
						if (ImGui::Button(("削除##" + std::to_string(i)).c_str())) {
							enemyEventManager_.RemoveEvent(i);
							break; 
						}
					}
				}

				ImGui::Separator();
				ImGui::Text("敵機ルート確認 (Editor Preview)");
				if (ImGui::Button("リセット")) ResetEditorPreview();
				ImGui::SameLine();
				if (ImGui::Button("再生")) isEditorPreviewPlaying_ = true;
				ImGui::SameLine();
				if (ImGui::Button("ストップ")) isEditorPreviewPlaying_ = false;
				ImGui::TextColored(isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "状態: %s", isEditorPreviewPlaying_ ? "再生中" : "停止中");
			}
			else if (currentSimulationTarget_ == 3) {
				ImGui::Text("=== GPU Particles ===");
				bool gpuChanged = false;
				if (auto *emitter = particleManager->GetEmitterSphere()) {
					if (ImGui::DragFloat3("位置", &emitter->translate.x, 0.01f)) gpuChanged = true;
					if (ImGui::DragFloat("射出半径", &emitter->radius, 0.01f)) gpuChanged = true;
					if (ImGui::DragInt("射出数", (int *) &emitter->count, 1, 0, 1000)) gpuChanged = true;
					if (ImGui::DragFloat("射出間隔", &emitter->frequency, 0.01f, 0.01f, 10.0f)) gpuChanged = true;
				}
				if (ImGui::Button("GPUパーティクルを再初期化") || gpuChanged) {
					particleManager->RequestGpuInitialize();
				}
				
				ImGui::Separator();
				ImGui::Text("=== Explosion Particles ===");
				if (explosionManager_) {
					auto& config = explosionManager_->GetConfig();
					ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
					ImGui::ColorEdit4("カラー", config.color);
					ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f);
					ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f);
					ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f);
					ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f);
					ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f);
					ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f);
					ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f);
					if (ImGui::Button("設定をJSONに保存")) explosionManager_->SaveToJson("resources/explosionConfig.json");
					ImGui::SameLine();
					if (ImGui::Button("設定をJSONから読込")) explosionManager_->LoadFromJson("resources/explosionConfig.json");
				}
			}
			else if (currentSimulationTarget_ == 4) {
				ImGui::Text("=== Camera Settings ===");
				if (isDebugCameraActive_) {
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[FREE CAM ACTIVE]");
					if (ImGui::Button("Switch to Player Camera")) SetDebugCameraActive(false);
					float moveSpd = debugFlyCamera_->GetMoveSpeed();
					float rotSpd  = debugFlyCamera_->GetRotateSpeed();
					float sens    = debugFlyCamera_->GetMouseSensitivity();
					float scroll  = debugFlyCamera_->GetScrollSpeed();
					float pan     = debugFlyCamera_->GetPanSpeed();
					if (ImGui::DragFloat("移動速度 (WASD)##fly", &moveSpd, 0.01f, 0.01f, 20.0f)) debugFlyCamera_->SetMoveSpeed(moveSpd);
					if (ImGui::DragFloat("回転感度 (マウス右)##fly", &sens, 0.0001f, 0.0001f, 0.05f, "%.4f")) debugFlyCamera_->SetMouseSensitivity(sens);
					if (ImGui::DragFloat("スクロール速度##fly", &scroll, 0.1f, 0.1f, 20.0f)) debugFlyCamera_->SetScrollSpeed(scroll);
					if (ImGui::DragFloat("パン速度 (中ボタン)##fly", &pan, 0.001f, 0.001f, 1.0f)) debugFlyCamera_->SetPanSpeed(pan);
					if (ImGui::DragFloat("回転速度 (キーボード)##fly",&rotSpd, 0.001f, 0.001f, 0.5f)) debugFlyCamera_->SetRotateSpeed(rotSpd);
					Vector3 flyPos = debugFlyCamera_->GetTranslate();
					float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
					if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
				} else {
					ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[PLAYER FOLLOW CAM]");
					if (ImGui::Button("Switch to Debug Camera")) SetDebugCameraActive(true);
					if (ImGui::Checkbox("Cinematic lock-on camera", &isCinematicLockOnCameraEnabled_)) isCinematicLockOnCameraInitialized_ = false;
					ImGui::Text("Cinematic: %s", (isCinematicLockOnCameraEnabled_ && lockedEnemy_) ? "ACTIVE" : "OFF");
					Vector3 camPos = camera->GetTranslate();
					float camPosArr[3] = { camPos.x, camPos.y, camPos.z };
					if (ImGui::DragFloat3("カメラ位置 (参考)##follow", camPosArr, 0.1f)) camera->SetTranslate({ camPosArr[0], camPosArr[1], camPosArr[2] });
				}
			}

			ImGui::End();
		}

		// --- オリジナルUI（今まで出していたImGui関連） ---
		//開発用UIの処理
		}
		ImGui::ShowDemoWindow();
		//ウィンドウのサイズを設定
		ImGui::SetNextWindowSize(ImVec2(500.0f, 400.0f), ImGuiCond_Once);

		//ウィンドウの作成
		ImGui::Begin("演習");

		ImGui::Text("表示設定");
		ImGui::Checkbox("スカイボックスを表示", &showSkybox);
		ImGui::Checkbox("平面を表示", &showPlane);
		ImGui::Checkbox("球体（ミサイル頭）を表示", &showSphere);
		ImGui::Checkbox("通常リングを表示", &showNormalRing);
		ImGui::Checkbox("部分リングを表示", &showPartialRing);
		ImGui::Checkbox("シリンダーを表示", &showCylinder);
		ImGui::Checkbox("トレイルを表示", &showTrail);
		ImGui::Checkbox("モデルを表示", &showModel);
		ImGui::Checkbox("パーティクルを表示", &showParticles);
		ImGui::Checkbox("ボーンを表示", &showBones);
		ImGui::Checkbox("スプライトを表示", &showSprite);
		ImGui::Checkbox("Show Debug Colliders", &showDebugColliders);

		ImGui::Separator();
		ImGui::Text("デバック　カメラ");
		if (ImGui::Button(isDebugCameraActive_ ? "Switch to Player Camera" : "Switch to Debug Camera")) {
			SetDebugCameraActive(!isDebugCameraActive_);
		}
		ImGui::SameLine();
		ImGui::Text("%s", isDebugCameraActive_ ? "Active: Debug Camera" : "Active: Player Camera");

		ImGui::Separator();
		ImGui::Text("GPUパーティクルの操作");
		bool gpuChanged = false;
		if (auto *emitter = particleManager->GetEmitterSphere()) {
			if (ImGui::DragFloat3("位置", &emitter->translate.x, 0.01f)) gpuChanged = true;
			if (ImGui::DragFloat("射出半径", &emitter->radius, 0.01f)) gpuChanged = true;
			if (ImGui::DragInt("射出数", (int *) &emitter->count, 1, 0, 1000)) gpuChanged = true;
			if (ImGui::DragFloat("射出間隔", &emitter->frequency, 0.01f, 0.01f, 10.0f)) gpuChanged = true;
		}

		if (ImGui::Button("GPUパーティクルを再初期化") || gpuChanged) {
			particleManager->RequestGpuInitialize();
		}

		ImGui::Separator();
		ImGui::Text("アニメーション制御");
		const char *animationNames[] = { "AnimatedCube", "simpleSkin", "sneakWalk", "walk" };
		if (ImGui::Combo("アニメーション", &currentAnimationIndex, animationNames, IM_ARRAYSIZE(animationNames))) {
			std::string dir, file, loadFile;
			if (currentAnimationIndex == 0) { dir = "resources/AnimatedCube"; file = "AnimatedCube.gltf"; loadFile = "AnimatedCube/AnimatedCube.gltf"; } else if (currentAnimationIndex == 1) { dir = "resources/simpleSkin"; file = "simpleSkin.gltf"; loadFile = "simpleSkin/simpleSkin.gltf"; } else if (currentAnimationIndex == 2) { dir = "resources/human"; file = "sneakWalk.gltf"; loadFile = "human/sneakWalk.gltf"; } else if (currentAnimationIndex == 3) { dir = "resources/human"; file = "walk.gltf"; loadFile = "human/walk.gltf"; }

			animationData = LoadAnimationFile(dir, file);
			Node rootNode = Model::LoadNodeHierarchy(dir, file);
			skeleton = CreateSkeleton(rootNode);
			if (!skeleton.joints.empty()) {
				skeleton.joints[skeleton.root].transform.translate = { 0.0f, 0.0f, 0.0f };
			}
			animationTime = 0.0f;

			ModelManager::GetInstance()->LoadModel(loadFile);
			myModelObject->SetModel(loadFile);
			if (myModelObject->GetModel()) {
				myModelObject->skinCluster = myModelObject->GetModel()->CreateSkinCluster(skeleton);
			}
			showModel = true;
			enableSkinning = true;

			// モデルに応じた適切なスケールを自動設定する
			if (currentAnimationIndex == 0) {
				modelScale = 1.0f;
			} else {
				modelScale = 1.0f;  // simpleSkinなどは等倍で人間サイズ
			}
		}
		ImGui::Checkbox("スキニング (ガワを動かす)", &enableSkinning);
		ImGui::SliderFloat("モデルスケール", &modelScale, 0.001f, 1.0f);
		if (ImGui::Checkbox("アニメーション再生", &playAnimation)) {}
		ImGui::SliderFloat("再生時間", &animationTime, 0.0f, animationData.duration);

		ImGui::Separator();
		ImGui::Text("シリンダー設定");
		ImGui::DragFloat3("シリンダー座標", cylinderPos, 0.01f);
		ImGui::DragFloat3("シリンダースケール", cylinderScale, 0.01f);
		ImGui::DragFloat2("UVスクロール速度", cylinderUVScrollSpeed, 0.001f);
		ImGui::SliderFloat("アルファリファレンス", &cylinderAlphaReference, 0.0f, 1.0f);

		ImGui::Separator();
		missilePresetManager_->DrawMissileSettingsUI();

		bool cChanged = false;
		if (ImGui::SliderInt("Subdivision##Cyl", &cylinderSubdivision, 3, 128)) cChanged = true;
		if (ImGui::SliderInt("Vertical Subdivision", &cylinderVerticalSubdivision, 1, 32)) cChanged = true;
		if (ImGui::DragFloat("Top Radius X", &cylinderTopRadiusX, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Top Radius Z", &cylinderTopRadiusZ, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Bottom Radius X", &cylinderBottomRadiusX, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Bottom Radius Z", &cylinderBottomRadiusZ, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Height", &cylinderHeight, 0.01f)) cChanged = true;
		if (ImGui::ColorEdit4("Top Color", cylinderTopColor)) cChanged = true;
		if (ImGui::ColorEdit4("Bottom Color", cylinderBottomColor)) cChanged = true;
		if (ImGui::SliderFloat("Start Angle##Cyl", &cylinderStartAngle, 0.0f, 360.0f)) cChanged = true;
		if (ImGui::SliderFloat("End Angle##Cyl", &cylinderEndAngle, 0.0f, 360.0f)) cChanged = true;
		if (ImGui::Checkbox("Flip UV", &cylinderIsUvFlipped)) cChanged = true;

		if (myCylinder && cChanged) {
			Model *model = myCylinder->GetModel();
			if (model) {
				model->InitializeCylinder(model->GetModelCommon(),
					cylinderSubdivision, cylinderVerticalSubdivision,
					cylinderTopRadiusX, cylinderTopRadiusZ,
					cylinderBottomRadiusX, cylinderBottomRadiusZ,
					cylinderHeight,
					{ cylinderTopColor[0], cylinderTopColor[1], cylinderTopColor[2], cylinderTopColor[3] },
					{ cylinderBottomColor[0], cylinderBottomColor[1], cylinderBottomColor[2], cylinderBottomColor[3] },
					cylinderStartAngle, cylinderEndAngle,
					cylinderIsUvFlipped);
			}
		}

		ImGui::Separator();
		ImGui::Text("Partial Ring Settings");

		bool pRingChanged = false;
		if (ImGui::SliderInt("Subdivision", &prSubdivision, 3, 128)) pRingChanged = true;
		if (ImGui::SliderFloat("Outer Radius", &prOuterRadius, 0.1f, 5.0f)) pRingChanged = true;
		if (ImGui::SliderFloat("Inner Radius", &prInnerRadius, 0.1f, 5.0f)) pRingChanged = true;
		if (ImGui::Checkbox("UV Horizontal", &prIsUvHorizontal)) pRingChanged = true;
		if (ImGui::ColorEdit4("Inner Color", prInnerColor)) pRingChanged = true;
		if (ImGui::ColorEdit4("Outer Color", prOuterColor)) pRingChanged = true;
		if (ImGui::SliderFloat("Start Angle", &prStartAngle, 0.0f, 360.0f)) pRingChanged = true;
		if (ImGui::SliderFloat("End Angle", &prEndAngle, 0.0f, 360.0f)) pRingChanged = true;
		if (ImGui::SliderFloat("Fade Angle", &prFadeAngle, 0.0f, 180.0f)) pRingChanged = true;

		if (pRingChanged && myPartialRing) {
			Model *model = myPartialRing->GetModel();
			if (model) {
				model->InitializeRing(
					model->GetModelCommon(), prSubdivision, prOuterRadius, prInnerRadius,
					prIsUvHorizontal, { prInnerColor[0], prInnerColor[1], prInnerColor[2], prInnerColor[3] },
					{ prOuterColor[0], prOuterColor[1], prOuterColor[2], prOuterColor[3] },
					prStartAngle, prEndAngle, prFadeAngle
				);
			}
		}

		//スプライトの座標を配列に格納
		Vector2 currentPos = sprite->GetPosition();
		float pos[2] = { currentPos.x,currentPos.y };

		//スライダーでスプライトの座標を変更できるようにする
		ImGui::SliderFloat2("Sprite Position", pos, 0.0f, 1280.0f, "%.1f");

		//スライダーで変更された座標をスプライトに反映
		sprite->SetPosition({ pos[0],pos[1] });

		ImGui::Separator();
		ImGui::Text("イベントツール（増援設定）");
        
		static int selectedTrigger = 0;
		static int selectedTarget = 0;
		static int eventDelay = 60;

		std::vector<const char*> enemyNames;
		for (const auto& sp : enemySpawns_) {
			enemyNames.push_back(sp.name.c_str());
		}

		if (!enemyNames.empty()) {
			if (selectedTrigger >= enemyNames.size()) selectedTrigger = 0;
			if (selectedTarget >= enemyNames.size()) selectedTarget = 0;

			ImGui::Combo("トリガーとなる敵", &selectedTrigger, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::Combo("出現する敵(ターゲット)", &selectedTarget, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::DragInt("出現までのディレイ(フレーム)", &eventDelay, 1, 0, 600);

			if (ImGui::Button("イベントを追加")) {
				enemyEventManager_.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
				enemySpawns_[selectedTarget].isInitialSpawn = false;
			}

			ImGui::SameLine();
			if (ImGui::Button("イベントを保存")) {
				enemyEventManager_.SaveEvents("resources/enemy_events.json");
			}

			ImGui::Separator();
			ImGui::Text("設定済みのイベント一覧");
			for (size_t i = 0; i < enemyEventManager_.GetEvents().size(); ++i) {
				const auto& ev = enemyEventManager_.GetEvents()[i];
				ImGui::Text("[%d] %s が死んだら %d F後に %s が出現", (int)i, ev.triggerEnemyName.c_str(), ev.delayFrames, ev.targetEnemyName.c_str());
				ImGui::SameLine();
				if (ImGui::Button(("削除##" + std::to_string(i)).c_str())) {
					enemyEventManager_.RemoveEvent(i);
					break; 
				}
			}
		}

		ImGui::End();

		ImGui::Begin("Camera Settings");

		// =====================================================
		if (isDebugCameraActive_) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[FREE CAM ACTIVE]");
			ImGui::Text("WASD: 移動  /  矢印キー: 回転  /  Q,E: ロール");
			ImGui::Text("Game View上で右ドラッグやWASDを使って確認できます");

			if (ImGui::Button("自機追従カメラに戻る")) {
				SetDebugCameraActive(false);
			}

			ImGui::Separator();
			ImGui::Text("操作方法:");
			ImGui::BulletText("右クリック + ドラッグ : 視点回転");
			ImGui::BulletText("スクロールホイール   : 前後移動");
			ImGui::BulletText("中クリック + ドラッグ : 上下左右パン");
			ImGui::BulletText("WASD                  : 前後左右移動");
			ImGui::BulletText("矢印キー              : 視点回転(キーボード)");
			ImGui::BulletText("Q / E                 : ロール");

			ImGui::Separator();
			// フリーカメラの速度調整（カメラから現在値を読む）
			float moveSpd = debugFlyCamera_->GetMoveSpeed();
			float rotSpd  = debugFlyCamera_->GetRotateSpeed();
			float sens    = debugFlyCamera_->GetMouseSensitivity();
			float scroll  = debugFlyCamera_->GetScrollSpeed();
			float pan     = debugFlyCamera_->GetPanSpeed();
			if (ImGui::DragFloat("移動速度 (WASD)##fly",      &moveSpd, 0.01f, 0.01f, 20.0f)) {
				debugFlyCamera_->SetMoveSpeed(moveSpd);
			}
			if (ImGui::DragFloat("回転感度 (マウス右)##fly",  &sens,    0.0001f, 0.0001f, 0.05f, "%.4f")) {
				debugFlyCamera_->SetMouseSensitivity(sens);
			}
			if (ImGui::DragFloat("スクロール速度##fly",       &scroll,  0.1f, 0.1f, 20.0f)) {
				debugFlyCamera_->SetScrollSpeed(scroll);
			}
			if (ImGui::DragFloat("パン速度 (中ボタン)##fly",  &pan,     0.001f, 0.001f, 1.0f)) {
				debugFlyCamera_->SetPanSpeed(pan);
			}
			if (ImGui::DragFloat("回転速度 (キーボード)##fly",&rotSpd,  0.001f, 0.001f, 0.5f)) {
				debugFlyCamera_->SetRotateSpeed(rotSpd);
			}

			ImGui::Separator();
			// フリーカメラの現在位置表示
			Vector3 flyPos = debugFlyCamera_->GetTranslate();
			float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
			if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) {
				debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
			}

		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[PLAYER FOLLOW CAM]");
			ImGui::Text("ボタンを押すとフリーカメラに切り替わります");

			if (ImGui::Button("フリーカメラに切り替え")) {
				SetDebugCameraActive(true);
			}

			ImGui::Separator();
			// 自機追従カメラの現在位置表示（参考用）
			if (ImGui::Checkbox("Cinematic lock-on camera", &isCinematicLockOnCameraEnabled_)) {
				isCinematicLockOnCameraInitialized_ = false;
			}
			ImGui::Text("Cinematic: %s", (isCinematicLockOnCameraEnabled_ && lockedEnemy_) ? "ACTIVE" : "OFF");
			ImGui::Separator();

			Vector3 camPos = camera->GetTranslate();
			float camPosArr[3] = { camPos.x, camPos.y, camPos.z };
			if (ImGui::DragFloat3("カメラ位置 (参考)##follow", camPosArr, 0.1f)) {
				camera->SetTranslate({ camPosArr[0], camPosArr[1], camPosArr[2] });
			}
		}

		ImGui::End();

		ImGui::Separator();

		ImGui::Begin("敵 & 障害物");
		ImGui::Text("=== ターゲット配置 ===");
		ImGui::Text("Lock-on: %s", lockedEnemy_ ? "LOCKED" : "NONE");
		ImGui::Text("Tab: lock target / X: unlock");
		ImGui::DragFloat3("出現座標 (X,Y,Z)", newEnemyPos, 1.0f);

		// ボタンを押した瞬間に、新しい敵をリストに追加！
		if (ImGui::Button("敵を生成する！")) {
			auto newEnemy = std::make_unique<Enemy>();
			newEnemy->Initialize({ newEnemyPos[0], newEnemyPos[1], newEnemyPos[2] });
			enemies_.push_back(std::move(newEnemy));
		}

		ImGui::Separator();
		ImGui::Text("=== 敵のリスト (総数: %d) ===", (int)enemies_.size());
		int index = 0;
		for (const auto& enemy : enemies_) {
			Vector3 pos = enemy->GetPosition();
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
			index++;
		}
		if (enemies_.empty()) {
			ImGui::Text("現在、敵は存在しません。");
		}

		ImGui::Separator();
		ImGui::Text("=== 障害物のリスト (総数: %d) ===", (int)obstacles_.size());
		
		if (ImGui::Button("障害物の設定をJSONに保存")) {
			simulationManager_->SaveCurrentSimulationLayoutToSceneJson("resources/scene.json");
		}
		if (!simulationSaveMessage_.empty()) {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", simulationSaveMessage_.c_str());
		}

		int obsIndex = 0;
		for (const auto& obstacle : obstacles_) {
			Vector3 pos = obstacle->GetPosition();
			Vector3 colOff = obstacle->GetCollisionOffset();
			Vector3 colScale = obstacle->GetCollisionScale();
			bool colEnabled = obstacle->IsCollisionEnabled();
			bool useMeshCol = obstacle->IsUseMeshCollider();

			ImGui::PushID(obsIndex);
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", obsIndex, pos.x, pos.y, pos.z);
			
			if (ImGui::Checkbox("Collision Enabled", &colEnabled)) {
				obstacle->SetCollisionEnabled(colEnabled);
			}
			if (ImGui::Checkbox("Use Mesh Collider", &useMeshCol)) {
				obstacle->SetUseMeshCollider(useMeshCol);
			}
			if (ImGui::DragFloat3("Collision Offset", &colOff.x, 0.1f)) {
				obstacle->SetCollisionOffset(colOff);
			}
			if (ImGui::DragFloat3("Collision Scale", &colScale.x, 0.05f)) {
				obstacle->SetCollisionScale(colScale);
			}
			ImGui::PopID();
			
			obsIndex++;
		}
		if (obstacles_.empty()) {
			ImGui::Text("現在、障害物は存在しません。");
		}
		ImGui::End();

		ImGui::Begin("敵撃破パーティクル設定");
		if (explosionManager_) {
			auto& config = explosionManager_->GetConfig();
			ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
			ImGui::ColorEdit4("カラー", config.color);
			ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f);
			ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f);
			ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f);

			ImGui::Separator();
			if (ImGui::Button("設定をJSONに保存")) {
				explosionManager_->SaveToJson("resources/explosionConfig.json");
			}
			ImGui::SameLine();
			if (ImGui::Button("設定をJSONから読込")) {
				explosionManager_->LoadFromJson("resources/explosionConfig.json");
			}
		}
		ImGui::End();

#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
		const StageValidation::Report &stageValidationReport = StageValidation::GetLastReport();
		if (stageValidationReport.HasMessages() || stageValidationReport.HasCheckItems()) {
			if (gShowStageValidationWindow) {
				DrawStageValidationWindow(stageValidationReport);
			}

			if (gShowStageValidationLabels) {
				Camera *validationCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
				if (validationCamera) {
					DrawStageValidationOverlay(stageValidationReport, validationCamera->GetViewProjectionMatrix());
				}
			}
		}
#endif

		ImGui::Begin("Level Editor Tools"); // 新しいウィンドウを作る場合

#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
		ImGui::Text("レベル検査表示");
		ImGui::Checkbox("検査一覧を表示", &gShowStageValidationWindow);
		ImGui::Checkbox("警告ラベルを表示", &gShowStageValidationLabels);
		ImGui::Separator();
#endif

		// もしボタンが押されたら { } の中が実行される
		if (ImGui::Button("Open Blender")) {
			// ここでBlenderを起動！
			ShellExecuteA(nullptr, "open", "resources\\stage.blend", nullptr, nullptr, SW_SHOW);
		}

		ImGui::Separator();
		ImGui::Text("敵機ルート確認");
		if (ImGui::Button("リセット")) {
			ResetEditorPreview();
		}
		ImGui::SameLine();
		if (ImGui::Button("再生")) {
			isEditorPreviewPlaying_ = true;
			OutputDebugStringA("[EditorPreview] Play.\n");
		}
		ImGui::SameLine();
		if (ImGui::Button("ストップ")) {
			isEditorPreviewPlaying_ = false;
			OutputDebugStringA("[EditorPreview] Stop.\n");
		}

		ImGui::TextColored(
			isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
			"状態: %s",
			isEditorPreviewPlaying_ ? "再生中" : "停止中");

		ImGui::TextWrapped("リセットでscene.jsonを読み直して初期状態に戻し、停止状態にします。再生で敵機ルートなどのゲーム更新が進み、ストップでその場に止まります。");

		ImGui::End();
	}
#endif
}
