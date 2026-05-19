#include "StageLoader.h"
#include "externals/json.hpp"
#include "Game/enemy/Enemy.h" 
#include <Windows.h>
#include <fstream>

using json = nlohmann::json;

bool StageLoader::LoadSceneJson(const std::string &filePath, std::list<std::unique_ptr<Enemy>> &enemies) {
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

			// 1. 位置座標を取得
			auto &trans = objData["transform"]["translation"];
			Vector3 position = { trans[0].get<float>(), trans[1].get<float>(), trans[2].get<float>() };

			auto &scaleData = objData["transform"]["scale"];
			Vector3 scale = { scaleData[0].get<float>(), scaleData[1].get<float>(), scaleData[2].get<float>() };

			

			// 2. 「敵」のデータなら、生成してリストに追加！
			if (objData.contains("enemy")) {
				std::string enemyType = objData["enemy"]["type"].get<std::string>();

				// 敵を実体化！
				auto newEnemy = std::make_unique<Enemy>();
				newEnemy->Initialize(position);
				newEnemy->SetScale(scale);
				enemies.push_back(std::move(newEnemy));

				OutputDebugStringA((" Enemy Spawned at: " + std::to_string(position.x) + "\n").c_str());
			}
		}
	}
	return true;
}
