#include "StageLoader.h"
#include "externals/json.hpp"
#include "Game/enemy/Enemy.h" 
#include "Game/obstacle/Obstacle.h"
#include "Game/Player/Player.h"
#include <Windows.h>
#include <fstream>

using json = nlohmann::json;

bool StageLoader::LoadSceneJson(
	const std::string &filePath, 
	std::list<std::unique_ptr<Enemy>> &enemies,
	std::list<std::unique_ptr<Obstacle>> &obstacles,
	Player *player,
	std::vector<Vector3> *enemySpawnPoints) {
	
	// ファイルを開く
	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		OutputDebugStringA((" [StageLoader] File not found: " + filePath + "\n").c_str());
		return false;
	}

	// JSONをパース（解読）
	json root;
	try {
		ifs >> root;
	} catch (const json::parse_error &e) {
		std::string errorMsg = " [StageLoader] Parse error: " + std::string(e.what()) + "\n";
		OutputDebugStringA(errorMsg.c_str());
		return false;
	}

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
			auto getModelName = [&]() {
				std::string modelName = "ObstacleBox";
				if (objData.contains("name")) {
					std::string objName = objData["name"].get<std::string>();
					size_t dotPos = objName.find('.');
					modelName = (dotPos != std::string::npos) ? objName.substr(0, dotPos) : objName;
				}
				return modelName;
			};

			if (objData.contains("category")) {
				std::string category = objData["category"].get<std::string>();

				if (category == "PLAYER") {
					if (player) {
						player->SetPosition(position);
						player->SetRotation(rotation);
					}
					continue;
				}

				if (category == "ENEMY") {
					if (enemySpawnPoints) {
						enemySpawnPoints->push_back(position);
					} else {
						auto newEnemy = std::make_unique<Enemy>();
						newEnemy->Initialize(position);
						newEnemy->SetRotation(rotation);
						enemies.push_back(std::move(newEnemy));
					}
					continue;
				}

				if (category == "OBSTACLE") {
					auto newObstacle = std::make_unique<Obstacle>();
					std::string modelName = getModelName();
					newObstacle->Initialize(modelName, position, rotation, scale);
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
				if (enemySpawnPoints) {
					enemySpawnPoints->push_back(position);
				} else {
					auto newEnemy = std::make_unique<Enemy>();
					newEnemy->Initialize(position);
					newEnemy->SetRotation(rotation);
					enemies.push_back(std::move(newEnemy));
				}

				OutputDebugStringA((" Enemy Spawned at: " + std::to_string(position.x) + "\n").c_str());
			}
			// 3. 「障害物」のデータなら、生成してリストに追加！
			else if (objData.contains("type") && objData["type"].get<std::string>() == "MESH") {
				std::string modelName = "ObstacleBox"; // デフォルト
				if (objData.contains("name")) {
					std::string objName = objData["name"].get<std::string>();
					
					// Blenderで名前が重複した際につくサフィックス（.001など）を切り取る
					size_t dotPos = objName.find('.');
					if (dotPos != std::string::npos) {
						modelName = objName.substr(0, dotPos);
					} else {
						modelName = objName;
					}
				}

				auto newObstacle = std::make_unique<Obstacle>();
				newObstacle->Initialize(modelName, position, rotation, scale);

				// ブレンダーで「StageBounds」という名前にした場合はステージの枠として扱う
				if (modelName.find("StageBounds") != std::string::npos) {
					newObstacle->SetStageBounds(true);
					OutputDebugStringA((" StageBounds Spawned at: " + std::to_string(position.x) + "\n").c_str());
				} else {
					OutputDebugStringA((" Obstacle Spawned at: " + std::to_string(position.x) + "\n").c_str());
				}

				obstacles.push_back(std::move(newObstacle));
			}
		}
	}
	return true;
}
