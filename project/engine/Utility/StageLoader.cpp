#include "StageLoader.h"
#include "StageValidation.h"
#include "externals/json.hpp"
#include "3D/ModelManager.h"
#include "Game/enemy/Enemy.h" 
#include "Game/obstacle/Obstacle.h"
#include "Game/Player/Player.h"
#include <Windows.h>
#include <fstream>
#include <unordered_map>

using json = nlohmann::json;

namespace {

std::wstring Utf8ToWide(const std::string& text) {
	if (text.empty()) {
		return {};
	}

	int wideSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, nullptr, 0);
	if (wideSize <= 0) {
		return std::wstring(text.begin(), text.end());
	}

	std::wstring wideText(static_cast<size_t>(wideSize), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, wideText.data(), wideSize);
	if (!wideText.empty() && wideText.back() == L'\0') {
		wideText.pop_back();
	}
	return wideText;
}

Vector3 ToGamePosition(const json &point) {
	return {
		point[0].get<float>(),
		point[2].get<float>(),
		point[1].get<float>()
	};
}

std::unordered_map<std::string, EnemyFlightPath> LoadFlightPaths(const json &root) {
	std::unordered_map<std::string, EnemyFlightPath> paths;

	if (!root.contains("paths") || !root["paths"].is_array()) {
		return paths;
	}

	for (const auto &pathData : root["paths"]) {
		if (!pathData.is_object() || !pathData.contains("id") || !pathData["id"].is_string()) {
			continue;
		}

		EnemyFlightPath path;
		path.loop = pathData.value("loop", false);
		path.speed = pathData.value("speed", 0.05f);

		if (pathData.contains("points") && pathData["points"].is_array()) {
			for (const auto &point : pathData["points"]) {
				if (point.is_array() && point.size() == 3) {
					path.points.push_back(ToGamePosition(point));
				}
			}
		}

		paths[pathData["id"].get<std::string>()] = std::move(path);
	}

	return paths;
}

EnemySpawnData BuildEnemySpawnData(const json &objData, const Vector3 &position, const Vector3 &rotation, const std::unordered_map<std::string, EnemyFlightPath> &paths) {
	EnemySpawnData spawnData;
	spawnData.name = objData.value("name", "UnknownEnemy");
	spawnData.position = position;
	spawnData.rotation = rotation;

	if (objData.contains("path_id") && objData["path_id"].is_string()) {
		auto pathIt = paths.find(objData["path_id"].get<std::string>());
		if (pathIt != paths.end()) {
			spawnData.flightPath = pathIt->second;
		}
	}

	if (objData.contains("reinforcement") && objData["reinforcement"].is_object()) {
		const auto &reinforcement = objData["reinforcement"];
		spawnData.reinforcementTriggerName = reinforcement.value("trigger", "");
		spawnData.reinforcementDelayFrames = reinforcement.value("delay", 0);
		if (spawnData.reinforcementDelayFrames < 0) {
			spawnData.reinforcementDelayFrames = 0;
		}
		spawnData.isInitialSpawn = !spawnData.HasReinforcementTrigger();
	}

	return spawnData;
}

std::string GetObjectBaseName(const json& objData) {
	if (!objData.contains("name") || !objData["name"].is_string()) {
		return "ObstacleBox";
	}

	std::string objName = objData["name"].get<std::string>();
	size_t dotPos = objName.find('.');
	return (dotPos != std::string::npos) ? objName.substr(0, dotPos) : objName;
}

bool ResourceFileExists(const std::string& fileName) {
	std::string filePath = "resources/" + fileName;
	std::wstring widePath = Utf8ToWide(filePath);
	DWORD attributes = GetFileAttributesW(widePath.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool TryLoadModelFile(const std::string& modelFile) {
	if (modelFile.empty()) {
		return false;
	}

	ModelManager* modelManager = ModelManager::GetInstance();
	if (modelManager->FindModel(modelFile) != nullptr) {
		return true;
	}

	if (!ResourceFileExists(modelFile)) {
		OutputDebugStringA((" [StageLoader] Model file not found: resources/" + modelFile + "\n").c_str());
		return false;
	}

	modelManager->LoadModel(modelFile);
	return modelManager->FindModel(modelFile) != nullptr;
}

std::string ResolveObstacleModelName(const json& objData) {
	std::string objectName = GetObjectBaseName(objData);
	if (objectName.find("StageBounds") != std::string::npos) {
		return objectName;
	}

	if (objData.contains("model") && objData["model"].is_string()) {
		std::string modelFile = objData["model"].get<std::string>();
		if (TryLoadModelFile(modelFile)) {
			return modelFile;
		}
	}

	return objectName;
}

void ApplyFlightPath(Enemy &enemy, const EnemySpawnData &spawnData) {
	if (spawnData.flightPath.IsValid()) {
		enemy.SetFlightPath(spawnData.flightPath.points, spawnData.flightPath.loop, spawnData.flightPath.speed);
	}
}

}

bool StageLoader::LoadSceneJson(
	const std::string &filePath, 
	std::list<std::unique_ptr<Enemy>> &enemies,
	std::list<std::unique_ptr<Obstacle>> &obstacles,
	Player *player,
	std::vector<EnemySpawnData> *enemySpawns) {
	
	// ファイルを開く
	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		OutputDebugStringA((" [StageLoader] File not found: " + filePath + "\n").c_str());
#ifdef CG2_ENABLE_STAGE_VALIDATION
		StageValidation::SetErrorReport(filePath, "scene.json が見つかりません: " + filePath);
#endif
		return false;
	}

	// JSONをパース（解読）
	json root;
	try {
		ifs >> root;
	} catch (const json::parse_error &e) {
		std::string errorMsg = " [StageLoader] Parse error: " + std::string(e.what()) + "\n";
		OutputDebugStringA(errorMsg.c_str());
#ifdef CG2_ENABLE_STAGE_VALIDATION
		StageValidation::SetErrorReport(filePath, "JSON の解析に失敗しました: " + std::string(e.what()));
#endif
		return false;
	}

#ifdef CG2_ENABLE_STAGE_VALIDATION
	const StageValidation::Report &validationReport = StageValidation::ValidateSceneJson(root, filePath);
	if (validationReport.HasErrors()) {
		return false;
	}
#endif

	std::unordered_map<std::string, EnemyFlightPath> flightPaths = LoadFlightPaths(root);

	// "objects" 配列をループで回す
	if (root.contains("objects") && root["objects"].is_array()) {

		for (auto &objData : root["objects"]) {

			// 1. 位置座標を取得 (BlenderのZをC++のYに、BlenderのYをC++のZに変換)
			auto &trans = objData["transform"]["translation"];
			Vector3 position = { trans[0].get<float>(), trans[2].get<float>(), trans[1].get<float>() };

			auto &scaleData = objData["transform"]["scale"];
			Vector3 scale = { scaleData[0].get<float>(), scaleData[2].get<float>(), scaleData[1].get<float>() };

			// 回転角を取得して度数法からラジアンへ変換 (BlenderのX->X, BlenderのZ->Y, BlenderのY->Z)
			Vector3 rotation = { 0.0f, 0.0f, 0.0f };
			if (objData["transform"].contains("rotation")) {
				auto &rotData = objData["transform"]["rotation"];
				float toRad = 3.14159265f / 180.0f;
				rotation = {
					rotData[0].get<float>() * toRad,
					rotData[2].get<float>() * toRad,
					rotData[1].get<float>() * toRad
				};
			}

			// 2. 「敵」のデータなら、生成してリストに追加！
			if (objData.contains("category")) {
				std::string category = objData["category"].get<std::string>();

				if (category == "PLAYER") {
					if (player) {
						if (objData.contains("model") && objData["model"].is_string()) {
							// std::string modelFile = objData["model"].get<std::string>();
							// if (TryLoadModelFile(modelFile) && player->GetModelName() != modelFile) {
							// 	player->Initialize(modelFile);
							// }
						}
						// player->SetScale(scale); // Playerは自身でスケールを管理するため、scene.json のスケールで上書きしない
						player->SetPosition(position);
						player->SetRotation(rotation);
					}
					continue;
				}

				if (category == "ENEMY") {
					EnemySpawnData spawnData = BuildEnemySpawnData(objData, position, rotation, flightPaths);
					if (enemySpawns) {
						enemySpawns->push_back(spawnData);
					} else {
						auto newEnemy = std::make_unique<Enemy>();
						newEnemy->Initialize(spawnData.position);
						newEnemy->SetRotation(spawnData.rotation);
						ApplyFlightPath(*newEnemy, spawnData);
						enemies.push_back(std::move(newEnemy));
					}
					continue;
				}

				if (category == "OBSTACLE") {
					auto newObstacle = std::make_unique<Obstacle>();
					std::string modelName = ResolveObstacleModelName(objData);
					newObstacle->Initialize(modelName, position, rotation, scale);

					if (objData.contains("useMeshCollider") && objData["useMeshCollider"].is_boolean()) {
						newObstacle->SetUseMeshCollider(objData["useMeshCollider"].get<bool>());
					}
					if (objData.contains("isCollisionEnabled") && objData["isCollisionEnabled"].is_boolean()) {
						newObstacle->SetCollisionEnabled(objData["isCollisionEnabled"].get<bool>());
					}
					if (objData.contains("collisionOffset") && objData["collisionOffset"].is_array()) {
						auto &arr = objData["collisionOffset"];
						if (arr.size() == 3) newObstacle->SetCollisionOffset({arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()});
					}
					if (objData.contains("collisionScale") && objData["collisionScale"].is_array()) {
						auto &arr = objData["collisionScale"];
						if (arr.size() == 3) newObstacle->SetCollisionScale({arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()});
					}

					// 地形モデルは強制的にメッシュコライダーを使用する
					std::string objectName = GetObjectBaseName(objData);
					if (objectName.find("Terrain") != std::string::npos || objectName.find("terrain") != std::string::npos) {
						newObstacle->SetUseMeshCollider(true);
					}

					if (modelName.find("StageBounds") != std::string::npos) {
						newObstacle->SetStageBounds(true);
						OutputDebugStringA((" StageBounds Spawned at: " + std::to_string(position.x) + "\n").c_str());
					}
					obstacles.push_back(std::move(newObstacle));
					continue;
				}
			}

			if (objData.contains("enemy")) {
				std::string enemyType = objData["enemy"]["type"].get<std::string>();

				// 敵を実体化！
				EnemySpawnData spawnData = BuildEnemySpawnData(objData, position, rotation, flightPaths);
				if (enemySpawns) {
					enemySpawns->push_back(spawnData);
				} else {
					auto newEnemy = std::make_unique<Enemy>();
					newEnemy->Initialize(spawnData.position);
					newEnemy->SetRotation(spawnData.rotation);
					ApplyFlightPath(*newEnemy, spawnData);
					enemies.push_back(std::move(newEnemy));
				}

				OutputDebugStringA((" Enemy Spawned at: " + std::to_string(position.x) + "\n").c_str());
			}

		}
	}
	return true;
}
