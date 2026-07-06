#include "SimulationManager.h"
#include "MissilePresetManager.h"
#include "LockOnManager.h"
#include "GamePlayScene.h"
#include "GamePlaySceneHelpers.h"
#include <externals/imgui/imgui.h>
#include <fstream>
#include "engine/Input/Input.h"
#include "engine/math/MyMath.h"

SimulationManager::SimulationManager(GamePlayScene* scene) : scene_(scene) {}

bool SimulationManager::SaveCurrentSimulationLayoutToSceneJson(const std::string &filePath) {
	json root;
	{
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) {
			scene_->simulationSaveMessage_ = "scene.json が見つからなぁE��め保存できませんでした";
			OutputDebugStringA(("[SimulationSave] File not found: " + filePath + "\n").c_str());
			return false;
		}

		try {
			ifs >> root;
		} catch (const std::exception &e) {
			scene_->simulationSaveMessage_ = "scene.json の読み込みに失敗しました";
			OutputDebugStringA(("[SimulationSave] JSON parse failed: " + std::string(e.what()) + "\n").c_str());
			return false;
		}
	}

	if (!root.contains("objects") || !root["objects"].is_array()) {
		scene_->simulationSaveMessage_ = "scene.json に objects がなぁE��め保存できませんでした";
		OutputDebugStringA("[SimulationSave] objects array not found.\n");
		return false;
	}

	json &objects = root["objects"];
	size_t playerObjectIndex = kInvalidSceneObjectIndex;
	std::vector<size_t> enemyObjectIndices;
	std::vector<size_t> obstacleObjectIndices;

	for (size_t index = 0; index < objects.size(); ++index) {
		const json &objectData = objects[index];
		if (IsScenePlayerObject(objectData) && playerObjectIndex == kInvalidSceneObjectIndex) {
			playerObjectIndex = index;
		}
		if (IsSceneEnemyObject(objectData)) {
			enemyObjectIndices.push_back(index);
		} else if (IsSceneObstacleObject(objectData)) {
			obstacleObjectIndices.push_back(index);
		}
	}

	size_t savedCount = 0;

	if (scene_->player_ && playerObjectIndex != kInvalidSceneObjectIndex) {
		const Vector3 playerRotation = EulerFromQuaternionXYZ(scene_->player_->GetQuaternion());
		WriteSceneTransform(objects[playerObjectIndex], scene_->player_->GetPosition(), &playerRotation, nullptr);
		++savedCount;
	}

	size_t obstacleIndex = 0;
	for (const auto &obstacle : scene_->obstacles_) {
		if (!obstacle || obstacleIndex >= obstacleObjectIndices.size()) {
			break;
		}

		const Vector3 obstacleRotation = obstacle->GetRotation();
		const Vector3 obstacleScale = obstacle->GetScale();
		WriteSceneTransform(
			objects[obstacleObjectIndices[obstacleIndex]],
			obstacle->GetPosition(),
			&obstacleRotation,
			&obstacleScale);
		++obstacleIndex;
		++savedCount;
	}

	for (const auto &enemy : scene_->enemies_) {
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		const Vector3 enemyPosition = enemy->GetPosition();
		const Vector3 enemyRotation = enemy->GetRotation();
		const size_t spawnPointIndex = enemy->GetSpawnPointIndex();

		if (spawnPointIndex != Enemy::kNoSpawnPoint && spawnPointIndex < enemyObjectIndices.size()) {
			WriteSceneTransform(objects[enemyObjectIndices[spawnPointIndex]], enemyPosition, &enemyRotation, nullptr);
			if (spawnPointIndex < scene_->enemySpawns_.size()) {
				scene_->enemySpawns_[spawnPointIndex].position = enemyPosition;
				scene_->enemySpawns_[spawnPointIndex].rotation = enemyRotation;
			}
			++savedCount;
			continue;
		}

		json newEnemy;
		newEnemy["type"] = "MESH";
		newEnemy["name"] = MakeUniqueSceneObjectName(objects, "SimEnemy");
		newEnemy["category"] = "ENEMY";
		newEnemy["enemy"] = { { "type", "VF1" } };
		newEnemy["vertices_count"] = 0;
		const Vector3 enemyScale = enemy->GetScale();
		WriteSceneTransform(newEnemy, enemyPosition, &enemyRotation, &enemyScale);

		objects.push_back(newEnemy);

		EnemySpawnData spawnData;
		spawnData.name = newEnemy["name"].get<std::string>();
		spawnData.position = enemyPosition;
		spawnData.rotation = enemyRotation;
		spawnData.isInitialSpawn = true;
		scene_->enemySpawns_.push_back(spawnData);
		enemy->SetSpawnPointIndex(scene_->enemySpawns_.size() - 1);
		enemyObjectIndices.push_back(objects.size() - 1);
		++savedCount;
	}

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		scene_->simulationSaveMessage_ = "scene.json を書き込めませんでした";
		OutputDebugStringA(("[SimulationSave] Failed to open for write: " + filePath + "\n").c_str());
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	try {
		scene_->lastJsonWriteTime_ = std::filesystem::last_write_time(filePath);
	} catch (...) {
	}

	scene_->simulationSaveMessage_ = "現在の配置めEscene.json に保存しました。実ゲームにも反映されます";
	OutputDebugStringA(("[SimulationSave] Saved " + std::to_string(savedCount) + " transforms.\n").c_str());
	return true;
}

void SimulationManager::RefreshSimulationActionNames() {
	scene_->simulationActionNames_.clear();

	std::ifstream ifs(kSimulationActionsFilePath);
	if (!ifs.is_open()) {
		scene_->selectedSimulationActionIndex_ = 0;
		return;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		scene_->selectedSimulationActionIndex_ = 0;
		return;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		scene_->selectedSimulationActionIndex_ = 0;
		return;
	}

	for (const auto &actionData : root["actions"]) {
		if (!actionData.is_object() || !actionData.contains("name") || !actionData["name"].is_string()) {
			continue;
		}

		scene_->simulationActionNames_.push_back(actionData["name"].get<std::string>());
	}

	if (scene_->selectedSimulationActionIndex_ >= static_cast<int>(scene_->simulationActionNames_.size())) {
		scene_->selectedSimulationActionIndex_ = scene_->simulationActionNames_.empty() ? 0 : static_cast<int>(scene_->simulationActionNames_.size()) - 1;
	}
}

bool SimulationManager::SaveNamedSimulationAction(const std::string &filePath, const std::string &actionName) {
	const std::string trimmedName = TrimActionName(actionName);
	if (trimmedName.empty()) {
		scene_->simulationActionMessage_ = "保存名を入力してください。";
		return false;
	}

	json root;
	{
		std::ifstream ifs(filePath);
		if (ifs.is_open()) {
			try {
				ifs >> root;
			} catch (...) {
				root = json::object();
			}
		}
	}

	if (!root.is_object()) {
		root = json::object();
	}
	if (!root.contains("actions") || !root["actions"].is_array()) {
		root["actions"] = json::array();
	}

	json action;
	action["name"] = trimmedName;

	if (scene_->player_) {
		json playerData;
		playerData["position"] = ToVector3Json(scene_->player_->GetPosition());
		playerData["rotation"] = ToVector3Json(EulerFromQuaternionXYZ(scene_->player_->GetQuaternion()));
		playerData["mode"] = static_cast<int>(scene_->player_->GetCurrentMode());

		json modeParams = json::array();
		for (int modeIndex = 0; modeIndex < 3; ++modeIndex) {
			modeParams.push_back(PlayerModeParamsToJson(scene_->player_->GetModeParams(static_cast<PlayerMode>(modeIndex))));
		}
		playerData["modeParams"] = modeParams;
		action["player"] = playerData;
	}

	json missileData;
	missileData["normalSpeed"] = scene_->missileNormalSpeed;
	missileData["normalScale"] = scene_->missileNormalScale;
	missileData["normalCollisionRadius"] = scene_->missileNormalCollisionRadius;
	missileData["normalLifeTime"] = scene_->missileNormalLifeTime;
	missileData["speed"] = scene_->missileSpeed;
	missileData["ampX"] = scene_->missileAmpX;
	missileData["ampZ"] = scene_->missileAmpZ;
	missileData["ampY"] = scene_->missileAmpY;
	missileData["freqY"] = scene_->missileFreqY;
	missileData["baseY"] = scene_->missileBaseY;
	missileData["homingStrength"] = scene_->missileHomingStrength;
	missileData["homingScale"] = scene_->missileHomingScale;
	missileData["homingCollisionRadius"] = scene_->missileHomingCollisionRadius;
	missileData["trailWidth"] = scene_->missileTrailWidth;
	missileData["lifeTime"] = scene_->missileLifeTime;
	missileData["muzzleOffset"] = scene_->missileMuzzleOffset;
	action["missile"] = missileData;

	json enemiesData = json::array();
	int generatedEnemyIndex = 1;
	for (const auto &enemy : scene_->enemies_) {
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		json enemyData;
		const size_t spawnPointIndex = enemy->GetSpawnPointIndex();
		if (spawnPointIndex < scene_->enemySpawns_.size()) {
			const EnemySpawnData &spawnData = scene_->enemySpawns_[spawnPointIndex];
			enemyData["name"] = spawnData.name;
			enemyData["reinforcementTrigger"] = spawnData.reinforcementTriggerName;
			enemyData["reinforcementDelay"] = spawnData.reinforcementDelayFrames;
		} else {
			enemyData["name"] = "SimEnemy" + std::to_string(generatedEnemyIndex++);
		}

		enemyData["position"] = ToVector3Json(enemy->GetPosition());
		enemyData["rotation"] = ToVector3Json(enemy->GetRotation());
		enemyData["scale"] = ToVector3Json(enemy->GetScale());
		enemyData["initial"] = true;
		enemiesData.push_back(enemyData);
	}
	action["enemies"] = enemiesData;

	json obstaclesData = json::array();
	for (const auto &obstacle : scene_->obstacles_) {
		if (!obstacle) {
			continue;
		}

		json obstacleData;
		obstacleData["position"] = ToVector3Json(obstacle->GetPosition());
		obstacleData["rotation"] = ToVector3Json(obstacle->GetRotation());
		obstacleData["scale"] = ToVector3Json(obstacle->GetScale());
		obstacleData["collisionOffset"] = ToVector3Json(obstacle->GetCollisionOffset());
		obstacleData["collisionScale"] = ToVector3Json(obstacle->GetCollisionScale());
		obstacleData["isCollisionEnabled"] = obstacle->IsCollisionEnabled();
		obstacleData["useMeshCollider"] = obstacle->IsUseMeshCollider();
		obstacleData["stageBounds"] = obstacle->IsStageBounds();
		obstaclesData.push_back(obstacleData);
	}
	action["obstacles"] = obstaclesData;

	if (scene_->explosionManager_) {
		const auto &config = scene_->explosionManager_->GetConfig();
		json explosionData;
		explosionData["count"] = config.count;
		explosionData["color"] = json::array({ config.color[0], config.color[1], config.color[2], config.color[3] });
		explosionData["speed"] = config.speed;
		explosionData["speedVariance"] = config.speedVariance;
		explosionData["scale"] = config.scale;
		explosionData["scaleVariance"] = config.scaleVariance;
		explosionData["lifeTimeMin"] = config.lifeTimeMin;
		explosionData["lifeTimeMax"] = config.lifeTimeMax;
		explosionData["posVariance"] = config.posVariance;
		action["explosion"] = explosionData;
	}

	json &actions = root["actions"];
	bool updated = false;
	for (auto &existingAction : actions) {
		if (existingAction.is_object() && existingAction.value("name", "") == trimmedName) {
			existingAction = action;
			updated = true;
			break;
		}
	}
	if (!updated) {
		actions.push_back(action);
	}

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		scene_->simulationActionMessage_ = "名前付き行動を保存できませんでした。";
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	RefreshSimulationActionNames();
	for (size_t index = 0; index < scene_->simulationActionNames_.size(); ++index) {
		if (scene_->simulationActionNames_[index] == trimmedName) {
			scene_->selectedSimulationActionIndex_ = static_cast<int>(index);
			break;
		}
	}

	scene_->simulationActionMessage_ = "行動「" + trimmedName + "」を保存しました。";
	return true;
}

bool SimulationManager::ApplySimulationAction(const std::string &filePath, const std::string &actionName) {
	const std::string trimmedName = TrimActionName(actionName);
	if (trimmedName.empty()) {
		scene_->simulationActionMessage_ = "読み込む行動名がありません。";
		return false;
	}

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		scene_->simulationActionMessage_ = "名前付き行動ファイルが見つかりません。";
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		scene_->simulationActionMessage_ = "名前付き行動ファイルを読み込めませんでした。";
		return false;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		scene_->simulationActionMessage_ = "保存された行動がありません。";
		return false;
	}

	const json *action = nullptr;
	for (const auto &actionData : root["actions"]) {
		if (actionData.is_object() && actionData.value("name", "") == trimmedName) {
			action = &actionData;
			break;
		}
	}

	if (!action) {
		scene_->simulationActionMessage_ = "選択した行動が見つかりません。";
		return false;
	}

	scene_->lockedEnemy_ = nullptr;
	scene_->aimAssistEnemy_ = nullptr;
	scene_->lockOnManager_->CancelMultiLock();
	scene_->isCinematicLockOnCameraInitialized_ = false;
	scene_->isGameOver_ = false;
	scene_->gameOverTimer_ = 0;

	if (scene_->player_ && action->contains("player") && (*action)["player"].is_object()) {
		const json &playerData = (*action)["player"];
		if (playerData.contains("modeParams") && playerData["modeParams"].is_array()) {
			const json &modeParams = playerData["modeParams"];
			for (int modeIndex = 0; modeIndex < 3 && modeIndex < static_cast<int>(modeParams.size()); ++modeIndex) {
				ApplyPlayerModeParamsFromJson(modeParams[modeIndex], scene_->player_->GetModeParams(static_cast<PlayerMode>(modeIndex)));
			}
		}

		const int modeIndex = std::clamp(ReadJsonInt(playerData, "mode", 0), 0, 2);
		scene_->player_->ChangeMode(static_cast<PlayerMode>(modeIndex));
		scene_->player_->SetPosition(ReadVector3Json(playerData.value("position", json::array()), scene_->player_->GetPosition()));
		scene_->player_->SetRotation(ReadVector3Json(playerData.value("rotation", json::array()), { 0.0f, 0.0f, 0.0f }));
	}

	if (action->contains("missile") && (*action)["missile"].is_object()) {
		const json &missileData = (*action)["missile"];
		scene_->missileNormalSpeed = ReadJsonFloat(missileData, "normalSpeed", scene_->missileNormalSpeed);
		scene_->missileNormalScale = ReadJsonFloat(missileData, "normalScale", scene_->missileNormalScale);
		scene_->missileNormalCollisionRadius = ReadJsonFloat(missileData, "normalCollisionRadius", scene_->missileNormalCollisionRadius);
		scene_->missileNormalLifeTime = ReadJsonInt(missileData, "normalLifeTime", scene_->missileNormalLifeTime);
		scene_->missileSpeed = ReadJsonFloat(missileData, "speed", scene_->missileSpeed);
		scene_->missileAmpX = ReadJsonFloat(missileData, "ampX", scene_->missileAmpX);
		scene_->missileAmpZ = ReadJsonFloat(missileData, "ampZ", scene_->missileAmpZ);
		scene_->missileAmpY = ReadJsonFloat(missileData, "ampY", scene_->missileAmpY);
		scene_->missileFreqY = ReadJsonFloat(missileData, "freqY", scene_->missileFreqY);
		scene_->missileBaseY = ReadJsonFloat(missileData, "baseY", scene_->missileBaseY);
		scene_->missileHomingStrength = ReadJsonFloat(missileData, "homingStrength", scene_->missileHomingStrength);
		scene_->missileHomingScale = ReadJsonFloat(missileData, "homingScale", scene_->missileHomingScale);
		scene_->missileHomingCollisionRadius = ReadJsonFloat(missileData, "homingCollisionRadius", scene_->missileHomingCollisionRadius);
		scene_->missileTrailWidth = ReadJsonFloat(missileData, "trailWidth", scene_->missileTrailWidth);
		scene_->missileLifeTime = ReadJsonInt(missileData, "lifeTime", scene_->missileLifeTime);
		scene_->missileMuzzleOffset = ReadJsonFloat(missileData, "muzzleOffset", scene_->missileMuzzleOffset);
	}

	if (scene_->explosionManager_ && action->contains("explosion") && (*action)["explosion"].is_object()) {
		const json &explosionData = (*action)["explosion"];
		auto &config = scene_->explosionManager_->GetConfig();
		config.count = ReadJsonInt(explosionData, "count", config.count);
		if (explosionData.contains("color") && explosionData["color"].is_array() && explosionData["color"].size() >= 4) {
			for (int index = 0; index < 4; ++index) {
				config.color[index] = explosionData["color"][index].get<float>();
			}
		}
		config.speed = ReadJsonFloat(explosionData, "speed", config.speed);
		config.speedVariance = ReadJsonFloat(explosionData, "speedVariance", config.speedVariance);
		config.scale = ReadJsonFloat(explosionData, "scale", config.scale);
		config.scaleVariance = ReadJsonFloat(explosionData, "scaleVariance", config.scaleVariance);
		config.lifeTimeMin = ReadJsonFloat(explosionData, "lifeTimeMin", config.lifeTimeMin);
		config.lifeTimeMax = ReadJsonFloat(explosionData, "lifeTimeMax", config.lifeTimeMax);
		config.posVariance = ReadJsonFloat(explosionData, "posVariance", config.posVariance);
	}

	if (action->contains("obstacles") && (*action)["obstacles"].is_array()) {
		auto obstacleIt = scene_->obstacles_.begin();
		for (const auto &obstacleData : (*action)["obstacles"]) {
			if (obstacleIt == scene_->obstacles_.end()) {
				break;
			}
			if (!(*obstacleIt)) {
				++obstacleIt;
				continue;
			}

			(*obstacleIt)->SetPosition(ReadVector3Json(obstacleData.value("position", json::array()), (*obstacleIt)->GetPosition()));
			(*obstacleIt)->SetRotation(ReadVector3Json(obstacleData.value("rotation", json::array()), (*obstacleIt)->GetRotation()));
			(*obstacleIt)->SetScale(ReadVector3Json(obstacleData.value("scale", json::array()), (*obstacleIt)->GetScale()));
			(*obstacleIt)->SetCollisionOffset(ReadVector3Json(obstacleData.value("collisionOffset", json::array()), (*obstacleIt)->GetCollisionOffset()));
			(*obstacleIt)->SetCollisionScale(ReadVector3Json(obstacleData.value("collisionScale", json::array()), (*obstacleIt)->GetCollisionScale()));
			(*obstacleIt)->SetCollisionEnabled(obstacleData.value("isCollisionEnabled", (*obstacleIt)->IsCollisionEnabled()));
			(*obstacleIt)->SetUseMeshCollider(obstacleData.value("useMeshCollider", (*obstacleIt)->IsUseMeshCollider()));
			(*obstacleIt)->Update();
			++obstacleIt;
		}
	}

	if (action->contains("enemies") && (*action)["enemies"].is_array()) {
		scene_->enemies_.clear();
		scene_->enemySpawns_.clear();

		for (const auto &enemyData : (*action)["enemies"]) {
			if (!enemyData.is_object()) {
				continue;
			}

			EnemySpawnData spawnData;
			spawnData.name = enemyData.value("name", "SavedEnemy");
			spawnData.position = ReadVector3Json(enemyData.value("position", json::array()), { 0.0f, 0.0f, 0.0f });
			spawnData.rotation = ReadVector3Json(enemyData.value("rotation", json::array()), { 0.0f, 0.0f, 0.0f });
			spawnData.isInitialSpawn = enemyData.value("initial", true);
			spawnData.reinforcementTriggerName = enemyData.value("reinforcementTrigger", "");
			spawnData.reinforcementDelayFrames = ReadJsonInt(enemyData, "reinforcementDelay", 0);
			scene_->enemySpawns_.push_back(spawnData);

			if (spawnData.isInitialSpawn) {
				auto enemy = std::make_unique<Enemy>();
				enemy->Initialize(spawnData.position);
				enemy->SetRotation(spawnData.rotation);
				enemy->SetScale(ReadVector3Json(enemyData.value("scale", json::array()), enemy->GetScale()));
				enemy->SetSpawnPointIndex(scene_->enemySpawns_.size() - 1);
				scene_->enemies_.push_back(std::move(enemy));
			}
		}
		scene_->enemyRespawnTimers_.assign(scene_->enemySpawns_.size(), kNoRespawnTimer);
	}

	if (scene_->missileManager_) {
		scene_->missileManager_->Initialize();
	}
	if (scene_->enemyBulletManager_) {
		scene_->enemyBulletManager_->Initialize();
	}

	if (!scene_->isDebugCameraActive_ && scene_->camera && scene_->player_) {
		Vector3 *targetPos = nullptr;
		Vector3 enemyPos;
		if (scene_->lockedEnemy_) {
			enemyPos = scene_->lockedEnemy_->GetPosition();
			targetPos = &enemyPos;
		}
		scene_->player_->UpdateCamera(scene_->camera.get(), targetPos);
		scene_->camera->Update();
	} else if (scene_->debugFlyCamera_) {
		scene_->debugFlyCamera_->Camera::Update();
	}

	Camera *activeCamera = scene_->isDebugCameraActive_ ? static_cast<Camera *>(scene_->debugFlyCamera_.get()) : scene_->camera.get();
	if (scene_->player_) {
		scene_->player_->UpdateModel();
	}
	for (auto &enemy : scene_->enemies_) {
		enemy->UpdateModel();
	}
	for (auto &obstacle : scene_->obstacles_) {
		obstacle->Update();
	}
	if (scene_->missileManager_) {
		scene_->missileManager_->UpdateModels(activeCamera);
	}

	scene_->simulationActionMessage_ = "設定「" + trimmedName + "」をゲームに読み込みました。";
	return true;
}

void SimulationManager::DrawSimulationScreenUI() {
#ifdef ENABLE_IMGUI
	const ImVec2 windowSize(
		static_cast<float>(WinApp::GetClientWidth()),
		static_cast<float>(WinApp::GetClientHeight()));
	float panelWidth = (windowSize.x > 560.0f) ? 520.0f : windowSize.x - 40.0f;
	if (panelWidth < 320.0f) {
		panelWidth = 320.0f;
	}
	float panelHeight = windowSize.y - 40.0f;
	if (panelHeight < 360.0f) {
		panelHeight = 360.0f;
	}
	float panelX = windowSize.x - panelWidth - 20.0f;
	if (panelX < 20.0f) {
		panelX = 20.0f;
	}
	ImGui::SetNextWindowPos(ImVec2(panelX, 20.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

	const ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse;

	if (!ImGui::Begin("シミュレーション画面", nullptr, windowFlags)) {
		ImGui::End();
		return;
	}

	ImGui::Text("シミュレーション画面");
	ImGui::SameLine();
	if (ImGui::Button("シミュレーションを閉じる (F2)")) {
		PostQuitMessage(0);
		ImGui::End();
		return;
	}
	ImGui::SameLine();
	if (ImGui::Button("リセット")) {
		scene_->ResetEditorPreview();
	}
	ImGui::SameLine();
	if (ImGui::Button("再生")) {
		scene_->isEditorPreviewPlaying_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("停止")) {
		scene_->isEditorPreviewPlaying_ = false;
	}
	ImGui::SameLine();
	ImGui::TextColored(
		scene_->isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
		"状態: %s",
		scene_->isEditorPreviewPlaying_ ? "再生中" : "停止中");

	ImGui::Separator();
	const char *previewModes[] = { "選択中だけ確認", "全体確認" };
	ImGui::Combo("確認モード", &scene_->simulationPlaybackMode_, previewModes, IM_ARRAYSIZE(previewModes));
	if (scene_->simulationPlaybackMode_ == 0) {
		ImGui::TextDisabled("今選んでいるカテゴリだけ動きます。ミサイルはテスト発射ボタンでだけ出ます。");
	} else {
		ImGui::TextDisabled("プレイヤー・敵・ミサイルをまとめて動かして全体の流れを確認します。");
	}

	ImGui::Separator();
	ImGui::Text("カメラ: %s", scene_->isDebugCameraActive_ ? "フリーカメラ" : "プレイヤー視点");
	ImGui::SameLine();
	if (scene_->isDebugCameraActive_) {
		if (ImGui::Button("プレイヤー視点にする (F3)")) {
			scene_->SetDebugCameraActive(false);
		}
	} else {
		if (ImGui::Button("フリーカメラに戻す (F3)")) {
			scene_->SetDebugCameraActive(true);
		}
	}

	DrawSimulationSaveControls();

	ImGui::Separator();
	const char *categories[] = { "プレイヤー", "ミサイル", "敵 & イベント", "パーティクル", "カメラ" };
	ImGui::Combo("カテゴリ", &scene_->currentSimulationTarget_, categories, IM_ARRAYSIZE(categories));
	ImGui::Separator();

	if (scene_->currentSimulationTarget_ == 0) {
		ImGui::Text("プレイヤー移動設定");
		if (scene_->player_) {
			auto mode = scene_->player_->GetCurrentMode();
			const char* modeName = (mode == PlayerMode::Fighter) ? "ファイター (1キー)" : 
								   (mode == PlayerMode::Gerwalk) ? "ガウォーク (2キー)" : "バトロイド (3キー)";
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "現在の形態: %s", modeName);
			
			PlayerModeParams& p = scene_->player_->GetModeParams(mode);
			ImGui::SliderFloat("最大移動速度", &p.maxMoveSpeed, 0.01f, 1.0f);
			ImGui::SliderFloat("移動加速度", &p.moveAcceleration, 0.001f, 0.1f);
			ImGui::SliderFloat("移動減衰", &p.moveDamping, 0.1f, 1.0f);
			ImGui::SliderFloat("ピッチ回転速度", &p.pitchSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ヨー回転速度", &p.yawSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ロール回転速度", &p.rollSpeed, 0.001f, 0.1f);

			ImGui::Separator();
			ImGui::Text("アニメーションデバッグ");
			bool isDebug = scene_->player_->IsAnimDebugActive();
			if (ImGui::Checkbox("デバッグ時間を強制", &isDebug)) {
				scene_->player_->SetAnimDebugActive(isDebug);
			}
			if (isDebug) {
				float t = scene_->player_->GetTargetAnimationTime();
				float duration = scene_->player_->GetAnimationDuration();
				if (ImGui::SliderFloat("アニメーション時間 (秒)", &t, 0.0f, duration, "%.3f秒")) {
					scene_->player_->SetTargetAnimationTime(t);
				}
				ImGui::Text("対応フレーム (24fps換算): %.1f", t * 24.0f);
			} else {
				ImGui::Text("アニメーション時間: %.3f秒 (目標: %.3f秒)", scene_->player_->GetAnimationTime(), scene_->player_->GetTargetAnimationTime());
			}
		} else {
			ImGui::Text("プレイヤーが初期化されていません。");
		}
	} else if (scene_->currentSimulationTarget_ == 1) {
		scene_->missilePresetManager_->DrawMissileSettingsUI();
	} else if (scene_->currentSimulationTarget_ == 2) {
		ImGui::Text("=== 敵の出現とルート ===");
		ImGui::Text("Lock-on: %s", scene_->lockedEnemy_ ? "LOCKED" : "NONE");
		ImGui::Text("Tab: ターゲットロック / X: ロック解除 / F2: シミュレーションを閉じる");
		ImGui::DragFloat3("出現座標 (X,Y,Z)", scene_->newEnemyPos, 1.0f);

		if (ImGui::Button("敵を生成する！")) {
			auto newEnemy = std::make_unique<Enemy>();
			newEnemy->Initialize({ scene_->newEnemyPos[0], scene_->newEnemyPos[1], scene_->newEnemyPos[2] });
			scene_->enemies_.push_back(std::move(newEnemy));
		}

		ImGui::Separator();
		ImGui::Text("敵のリスト (総数: %d)", static_cast<int>(scene_->enemies_.size()));
		int index = 0;
		for (const auto &enemy : scene_->enemies_) {
			Vector3 pos = enemy->GetPosition();
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
			++index;
		}

		ImGui::Separator();
		ImGui::Text("イベントツール（増援設定）");
		static int selectedTrigger = 0;
		static int selectedTarget = 0;
		static int eventDelay = 60;
		std::vector<const char *> enemyNames;
		for (const auto &spawn : scene_->enemySpawns_) {
			enemyNames.push_back(spawn.name.c_str());
		}

		if (!enemyNames.empty()) {
			if (selectedTrigger >= static_cast<int>(enemyNames.size())) {
				selectedTrigger = 0;
			}
			if (selectedTarget >= static_cast<int>(enemyNames.size())) {
				selectedTarget = 0;
			}

			ImGui::Combo("トリガーとなる敵", &selectedTrigger, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::Combo("出現する敵(ターゲット)", &selectedTarget, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::DragInt("出現までのディレイ(フレーム)", &eventDelay, 1, 0, 600);
			if (ImGui::Button("イベントを追加")) {
				scene_->enemyEventManager_.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
				scene_->enemySpawns_[selectedTarget].isInitialSpawn = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("イベントを保存")) {
				scene_->enemyEventManager_.SaveEvents("resources/enemy_events.json");
			}

			for (size_t i = 0; i < scene_->enemyEventManager_.GetEvents().size(); ++i) {
				const auto &event = scene_->enemyEventManager_.GetEvents()[i];
				ImGui::Text("[%d] %s が死んだら %d F後に %s が出現",
					static_cast<int>(i),
					event.triggerEnemyName.c_str(),
					event.delayFrames,
					event.targetEnemyName.c_str());
				ImGui::SameLine();
				if (ImGui::Button(("削除##event" + std::to_string(i)).c_str())) {
					scene_->enemyEventManager_.RemoveEvent(i);
					break;
				}
			}
		} else {
			ImGui::Text("敵の出現データがありません。");
		}
	} else if (scene_->currentSimulationTarget_ == 3) {
		ImGui::Text("=== GPUパーティクル ===");
		bool gpuChanged = false;
		if (scene_->particleManager) {
			if (auto *emitter = scene_->particleManager->GetEmitterSphere()) {
				if (ImGui::DragFloat3("位置", &emitter->translate.x, 0.01f)) gpuChanged = true;
				if (ImGui::DragFloat("射出半径", &emitter->radius, 0.01f)) gpuChanged = true;
				if (ImGui::DragInt("射出数", reinterpret_cast<int *>(&emitter->count), 1, 0, 1000)) gpuChanged = true;
				if (ImGui::DragFloat("射出間隔", &emitter->frequency, 0.01f, 0.01f, 10.0f)) gpuChanged = true;
			}
			if (ImGui::Button("GPUパーティクルを再初期化") || gpuChanged) {
				scene_->particleManager->RequestGpuInitialize();
			}
		}

		ImGui::Separator();
		ImGui::Text("=== 爆発パーティクル ===");
		if (scene_->explosionManager_) {
			auto &config = scene_->explosionManager_->GetConfig();
			ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
			ImGui::ColorEdit4("カラー", config.color);
			ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f);
			ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f);
			ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f);
			if (ImGui::Button("設定をJSONに保存")) {
				scene_->explosionManager_->SaveToJson("resources/explosionConfig.json");
			}
			ImGui::SameLine();
			if (ImGui::Button("設定をJSONから読込")) {
				scene_->explosionManager_->LoadFromJson("resources/explosionConfig.json");
			}
		}
	} else if (scene_->currentSimulationTarget_ == 4) {
		ImGui::Text("カメラ設定");
		if (scene_->isDebugCameraActive_) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[フリーカメラ アクティブ]");
			if (ImGui::Button("プレイヤー視点にする")) {
				scene_->SetDebugCameraActive(false);
			}
			float moveSpeed = scene_->debugFlyCamera_->GetMoveSpeed();
			float rotateSpeed = scene_->debugFlyCamera_->GetRotateSpeed();
			float sensitivity = scene_->debugFlyCamera_->GetMouseSensitivity();
			float scrollSpeed = scene_->debugFlyCamera_->GetScrollSpeed();
			float panSpeed = scene_->debugFlyCamera_->GetPanSpeed();
			if (ImGui::DragFloat("移動速度 (WASD)##fly", &moveSpeed, 0.01f, 0.01f, 20.0f)) scene_->debugFlyCamera_->SetMoveSpeed(moveSpeed);
			if (ImGui::DragFloat("回転感度 (マウス右)##fly", &sensitivity, 0.0001f, 0.0001f, 0.05f, "%.4f")) scene_->debugFlyCamera_->SetMouseSensitivity(sensitivity);
			if (ImGui::DragFloat("スクロール速度##fly", &scrollSpeed, 0.1f, 0.1f, 20.0f)) scene_->debugFlyCamera_->SetScrollSpeed(scrollSpeed);
			if (ImGui::DragFloat("パン速度 (中ボタン)##fly", &panSpeed, 0.001f, 0.001f, 1.0f)) scene_->debugFlyCamera_->SetPanSpeed(panSpeed);
			if (ImGui::DragFloat("回転速度 (キーボード)##fly", &rotateSpeed, 0.001f, 0.001f, 0.5f)) scene_->debugFlyCamera_->SetRotateSpeed(rotateSpeed);

			Vector3 flyPos = scene_->debugFlyCamera_->GetTranslate();
			float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
			if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) {
				scene_->debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
			}
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[プレイヤー視点 アクティブ]");
			if (ImGui::Button("フリーカメラに切り替え")) {
				scene_->SetDebugCameraActive(true);
			}
			if (ImGui::Checkbox("Cinematic lock-on scene_->camera", &scene_->isCinematicLockOnCameraEnabled_)) {
				scene_->isCinematicLockOnCameraInitialized_ = false;
			}
			ImGui::Text("Cinematic: %s", (scene_->isCinematicLockOnCameraEnabled_ && scene_->lockedEnemy_) ? "ACTIVE" : "OFF");
		}
	}

	ImGui::End();
#endif
}

void SimulationManager::DrawSimulationSaveControls() {
#ifdef ENABLE_IMGUI
	ImGui::Separator();
	ImGui::InputText("保存名", scene_->simulationActionName_, IM_ARRAYSIZE(scene_->simulationActionName_));
	if (ImGui::Button("名前を付けて行動を保存")) {
		SaveNamedSimulationAction(kSimulationActionsFilePath, scene_->simulationActionName_);
	}
	ImGui::SameLine();
	if (ImGui::Button("保存一覧を更新")) {
		RefreshSimulationActionNames();
	}

	if (!scene_->simulationActionMessage_.empty()) {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.45f, 1.0f), "%s", scene_->simulationActionMessage_.c_str());
	}
#endif
}

