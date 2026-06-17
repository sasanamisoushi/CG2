#pragma once
#include <string>
#include <list>
#include <memory>
#include <vector>
#include "engine/math/MyMath.h"

class Enemy;
class Obstacle;
class Player;

struct EnemyFlightPath {
	std::vector<Vector3> points;
	bool loop = false;
	float speed = 0.05f;

	bool IsValid() const { return points.size() >= 2; }
};

struct EnemySpawnData {
	std::string name; // Blenderでのオブジェクト名
	bool isInitialSpawn = true; // 最初から出現するかどうか
	Vector3 position = { 0.0f, 0.0f, 0.0f };
	Vector3 rotation = { 0.0f, 0.0f, 0.0f };
	EnemyFlightPath flightPath;
	std::string reinforcementTriggerName;
	int reinforcementDelayFrames = 0;

	bool HasReinforcementTrigger() const { return !reinforcementTriggerName.empty(); }
};

class StageLoader {
public:
	// Blenderから出力したJSONを読み込み、敵・障害物リストに追加する
	static bool LoadSceneJson(
		const std::string &filePath, 
		std::list<std::unique_ptr<Enemy>> &enemies,
		std::list<std::unique_ptr<Obstacle>> &obstacles,
		Player *player = nullptr,
		std::vector<EnemySpawnData> *enemySpawns = nullptr);
};
