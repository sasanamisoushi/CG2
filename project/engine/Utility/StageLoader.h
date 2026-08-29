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
	bool isBoss = false; // ボスかどうか
	bool isJammer = false; // ジャミング敵かどうか
	bool isGround = false; // 地上雑魚敵かどうか
	bool isInitialSpawn = true; // 最初から出現するかどうか
	bool hasSpawned = false; // すでに出現（スポーン）したかどうか
	Vector3 position = { 0.0f, 0.0f, 0.0f };
	Vector3 rotation = { 0.0f, 0.0f, 0.0f };
	EnemyFlightPath flightPath;
	std::string reinforcementTriggerName; // Raw comma-separated string
	std::vector<std::string> reinforcementTriggerNames; // Parsed list
	std::vector<std::string> remainingReinforcementTriggers; // Runtime state
	int reinforcementDelayFrames = 0;

	bool HasReinforcementTrigger() const { return !reinforcementTriggerNames.empty(); }
};

class StageLoader {
public:
	// 単一共通の敵種別判定関数 (全プログラムで統一使用して先祖返りを100%遮断)
	static void DetermineEnemyTypeFlags(const std::string& typeStr, const std::string& nameStr, bool& outIsGround, bool& outIsJammer, bool& outIsBoss);

	// Blenderから出力したJSONを読み込み、敵・障害物リストに追加する
	static bool LoadSceneJson(
		const std::string &filePath, 
		std::list<std::unique_ptr<Enemy>> &enemies,
		std::list<std::unique_ptr<Obstacle>> &obstacles,
		Player *player = nullptr,
		std::vector<EnemySpawnData> *enemySpawns = nullptr);
};
