#pragma once

#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/enemy/EnemyEventManager.h"
#include "Game/obstacle/Obstacle.h"
#include "Game/Player/Player.h"
#include "engine/Utility/StageLoader.h"
#include <list>
#include <vector>
#include <memory>
#include <string>

// レベル（敵のスポーンや障害物など）を管理するクラス
class LevelManager {
public:
	LevelManager();
	~LevelManager() = default;

	void Initialize(Player* player);
	void Update(std::vector<Vector3>& enemyBulletHits);
	void Draw();
	
	// 敵のリストを取得
	std::list<std::unique_ptr<Enemy>>& GetEnemies() { return enemies_; }
	// 障害物のリストを取得
	std::list<std::unique_ptr<Obstacle>>& GetObstacles() { return obstacles_; }
	
	EnemyBulletManager* GetEnemyBulletManager() const { return enemyBulletManager_.get(); }
	EnemyEventManager* GetEnemyEventManager() { return &enemyEventManager_; }

	// スポーンデータの取得・設定
	std::vector<EnemySpawnData>& GetEnemySpawns() { return enemySpawns_; }
	void SetEnemySpawns(const std::vector<EnemySpawnData>& spawns) { enemySpawns_ = spawns; }

	// ロックオンされている敵
	Enemy* GetLockedEnemy() const { return lockedEnemy_; }
	void SetLockedEnemy(Enemy* enemy) { lockedEnemy_ = enemy; }

	// 敵のスポーン関連
	void SpawnEnemiesFromSpawnPoints();
	void SpawnEnemyFromSpawnPoint(size_t spawnPointIndex);
	void ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames);
	void TriggerEnemyReinforcements(const std::string& deadEnemyName);
	void UpdateEnemyRespawns();
	bool IsEnemySpawnPointActive(size_t spawnPointIndex) const;

	// 新しい敵の追加など
	void AddEnemy(std::unique_ptr<Enemy> enemy);

private:
	Player* player_ = nullptr;

	std::list<std::unique_ptr<Enemy>> enemies_;
	std::unique_ptr<EnemyBulletManager> enemyBulletManager_;
	std::vector<EnemySpawnData> enemySpawns_;
	std::vector<int> enemyRespawnTimers_;
	EnemyEventManager enemyEventManager_;
	Enemy* lockedEnemy_ = nullptr;

	std::list<std::unique_ptr<Obstacle>> obstacles_;
};
