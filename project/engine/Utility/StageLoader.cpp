#include "StageLoader.h"
#include "StageValidation.h"
#include "externals/json.hpp"
#include "3D/ModelManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/JammerEnemy.h"
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

std::vector<std::string> SplitStringByComma(const std::string& str) {
	std::vector<std::string> result;
	size_t start = 0;
	size_t end = str.find(',');
	while (end != std::string::npos) {
		std::string token = str.substr(start, end - start);
		// trim
		token.erase(0, token.find_first_not_of(" \t"));
		token.erase(token.find_last_not_of(" \t") + 1);
		if (!token.empty()) {
			result.push_back(token);
		}
		start = end + 1;
		end = str.find(',', start);
	}
	std::string token = str.substr(start);
	token.erase(0, token.find_first_not_of(" \t"));
	token.erase(token.find_last_not_of(" \t") + 1);
	if (!token.empty()) {
		result.push_back(token);
	}
	return result;
}

EnemySpawnData BuildEnemySpawnData(const json &objData, const Vector3 &position, const Vector3 &rotation, const std::unordered_map<std::string, EnemyFlightPath> &paths) {
	EnemySpawnData spawnData;
	spawnData.name = objData.value("name", "UnknownEnemy");
	spawnData.position = position;
	spawnData.rotation = rotation;

	std::string typeStr = "";
	if (objData.contains("enemy") && objData["enemy"].is_object() && objData["enemy"].contains("type") && objData["enemy"]["type"].is_string()) {
		typeStr = objData["enemy"]["type"].get<std::string>();
	} else if (objData.contains("enemy_type") && objData["enemy_type"].is_string()) {
		typeStr = objData["enemy_type"].get<std::string>();
	}

	std::string upperType = typeStr;
	for (char &c : upperType) { c = static_cast<char>(toupper(static_cast<unsigned char>(c))); }
	std::string upperName = spawnData.name;
	for (char &c : upperName) { c = static_cast<char>(toupper(static_cast<unsigned char>(c))); }

	// 1. UIで明示的に指定された敵のタイプ (VF3, VF1, VF2, Boss等) を最優先で判定
	if (upperType == "VF3" || upperType == "GROUND" || upperType == "GROUNDENEMY" || upperType == "LAND") {
		spawnData.isGround = true;
		spawnData.isJammer = false;
		spawnData.isBoss = false;
	} else if (upperType == "VF1" || upperType == "FLY" || upperType == "AIR") {
		spawnData.isGround = false;
		spawnData.isJammer = false;
		spawnData.isBoss = false;
	} else if (upperType == "VF2" || upperType == "JAMMER") {
		spawnData.isJammer = true;
		spawnData.isGround = false;
		spawnData.isBoss = false;
	} else if (upperType == "BOSS") {
		spawnData.isBoss = true;
		spawnData.isGround = false;
		spawnData.isJammer = false;
		spawnData.isInitialSpawn = false;
	} else {
		// 2. 敵のタイプが未指定の場合のみ、オブジェクト名から判定
		if (upperName.find("BOSS") != std::string::npos) {
			spawnData.isBoss = true;
			spawnData.isGround = false;
			spawnData.isJammer = false;
			spawnData.isInitialSpawn = false;
		} else if (upperName.find("VF2") != std::string::npos || upperName.find("JAMMER") != std::string::npos) {
			spawnData.isJammer = true;
			spawnData.isGround = false;
			spawnData.isBoss = false;
		} else if (upperName.find("VF1") != std::string::npos) {
			spawnData.isGround = false;
			spawnData.isJammer = false;
			spawnData.isBoss = false;
		} else {
			// デフォルトは地上敵(GroundEnemy)
			spawnData.isGround = true;
			spawnData.isJammer = false;
			spawnData.isBoss = false;
		}
	}

	bool hasExplicitInitialSpawnSetting = false;
	bool explicitInitialSpawnValue = true;

	auto parseInitialSpawnFlag = [&](const json &container) {
		if (container.contains("is_initial_spawn") && container["is_initial_spawn"].is_boolean()) {
			hasExplicitInitialSpawnSetting = true;
			explicitInitialSpawnValue = container["is_initial_spawn"].get<bool>();
		} else if (container.contains("isInitialSpawn") && container["isInitialSpawn"].is_boolean()) {
			hasExplicitInitialSpawnSetting = true;
			explicitInitialSpawnValue = container["isInitialSpawn"].get<bool>();
		}
	};

	parseInitialSpawnFlag(objData);

	if (objData.contains("enemy") && objData["enemy"].is_object()) {
		const auto &enemyObj = objData["enemy"];
		parseInitialSpawnFlag(enemyObj);
	}

	if (objData.contains("path_id") && objData["path_id"].is_string()) {
		auto pathIt = paths.find(objData["path_id"].get<std::string>());
		if (pathIt != paths.end()) {
			spawnData.flightPath = pathIt->second;
		}
	}

	if (objData.contains("reinforcement") && objData["reinforcement"].is_object()) {
		const auto &reinforcement = objData["reinforcement"];
		parseInitialSpawnFlag(reinforcement);
		spawnData.reinforcementTriggerName = reinforcement.value("trigger", "");
		spawnData.reinforcementTriggerNames = SplitStringByComma(spawnData.reinforcementTriggerName);
		spawnData.remainingReinforcementTriggers = spawnData.reinforcementTriggerNames;
		
		spawnData.reinforcementDelayFrames = reinforcement.value("delay", 0);
		if (spawnData.reinforcementDelayFrames < 0) {
			spawnData.reinforcementDelayFrames = 0;
		}
	}

	if (spawnData.isBoss) {
		spawnData.isInitialSpawn = false;
	} else if (hasExplicitInitialSpawnSetting) {
		spawnData.isInitialSpawn = explicitInitialSpawnValue;
	} else {
		// ボス以外のすべての配置敵は、デフォルトで最初から全員（7体）出現させる
		spawnData.isInitialSpawn = true;
	}

	return spawnData;

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

			// 2. カテゴリの取得と、未設定時のオブジェクト名による自動救済判別
			std::string name = objData.value("name", "");
			std::string category = "";
			if (objData.contains("category") && objData["category"].is_string()) {
				category = objData["category"].get<std::string>();
			} else if (objData.contains("game_obj_type") && objData["game_obj_type"].is_string()) {
				category = objData["game_obj_type"].get<std::string>();
			}

			std::string upperName = name;
			for (char &c : upperName) { c = static_cast<char>(toupper(static_cast<unsigned char>(c))); }

			if (category.empty() || category == "NONE") {
				if (upperName.find("PLAYER") != std::string::npos) {
					category = "PLAYER";
				} else if (upperName.find("ENEMY") != std::string::npos || upperName.find("VF3") != std::string::npos || upperName.find("VF1") != std::string::npos || upperName.find("BOSS") != std::string::npos) {
					category = "ENEMY";
				} else if (upperName.find("TERRAIN") != std::string::npos || upperName.find("STAGE") != std::string::npos || upperName.find("OBSTACLE") != std::string::npos || upperName.find("BLOCK") != std::string::npos || upperName.find("OBJECT") != std::string::npos) {
					category = "OBSTACLE";
				}
			}

			if (category == "PLAYER") {
				if (player) {
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
					std::unique_ptr<Enemy> newEnemy;
					if (spawnData.isJammer) newEnemy = std::make_unique<JammerEnemy>();
					else newEnemy = std::make_unique<Enemy>();
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
				
				Vector3 initPos = position;
				Vector3 initRot = rotation;
				Vector3 initScale = scale;
				std::string objectName = GetObjectBaseName(objData);
				newObstacle->Initialize(modelName, initPos, initRot, initScale);

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
				objectName = GetObjectBaseName(objData);
				if (objectName.find("Terrain") != std::string::npos || objectName.find("terrain") != std::string::npos) {
					newObstacle->SetUseMeshCollider(true);
				}

				if (modelName.find("StageBounds") != std::string::npos) {
					newObstacle->SetStageBounds(true);
				}
				obstacles.push_back(std::move(newObstacle));
				continue;
			}
		}
	}
	return true;
}
