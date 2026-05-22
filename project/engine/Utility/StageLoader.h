#pragma once
#include <string>
#include <list>
#include <memory>
#include <vector>
#include "engine/math/MyMath.h"

class Enemy;
class Obstacle;
class Player;

class StageLoader {
public:
	// Blenderから出力したJSONを読み込み、敵・障害物リストに追加する
	static bool LoadSceneJson(
		const std::string &filePath, 
		std::list<std::unique_ptr<Enemy>> &enemies,
		std::list<std::unique_ptr<Obstacle>> &obstacles,
		Player *player = nullptr,
		std::vector<Vector3> *enemySpawnPoints = nullptr);
};
