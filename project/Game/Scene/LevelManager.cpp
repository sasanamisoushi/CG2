#include "LevelManager.h"
#include <algorithm>

namespace {
	constexpr int kNoRespawnTimer = -1;
}

LevelManager::LevelManager() {
	enemyBulletManager_ = std::make_unique<EnemyBulletManager>();
}

void LevelManager::Initialize(Player* player) {
	player_ = player;
	enemyBulletManager_->Initialize();
	enemies_.clear();
	obstacles_.clear();
	enemySpawns_.clear();
	enemyRespawnTimers_.clear();
	lockedEnemy_ = nullptr;
}

void LevelManager::Update(std::vector<Vector3>& enemyBulletHits) {
	UpdateEnemyRespawns();

	// 敵の更新
	for (auto it = enemies_.begin(); it != enemies_.end(); ) {
		Enemy* enemy = it->get();
		if (!enemy) {
			it = enemies_.erase(it);
			continue;
		}

		try {
			if (enemy->IsDead()) {
				// 敵が死んだ場合の増援チェック
				std::string deadEnemyName;
				for (const auto& spawnData : enemySpawns_) {
					if (spawnData.isInitialSpawn) { // 簡易的なチェック
						// TODO: 正確な名前の照合が必要
					}
				}
				
				// 爆発エフェクトなどはScene側で行うか、Managerで行うか
				it = enemies_.erase(it);
				continue;
			}
			
			enemy->Update(player_ ? player_->GetPosition() : Vector3{0,0,0}, enemyBulletManager_.get(), obstacles_);
			++it;
		} catch (...) {
			it = enemies_.erase(it);
		}
	}

	// 敵の弾の更新
	if (player_) {
		enemyBulletManager_->Update(player_, enemyBulletHits, obstacles_);
	}

	// 障害物の更新
	for (auto& obstacle : obstacles_) {
		if (obstacle) {
			obstacle->Update();
		}
	}
}

void LevelManager::Draw() {
	for (const auto& enemy : enemies_) {
		if (enemy) {
			enemy->Draw();
		}
	}

	for (const auto& obstacle : obstacles_) {
		if (obstacle) {
			obstacle->Draw();
		}
	}

	enemyBulletManager_->Draw();
}

void LevelManager::AddEnemy(std::unique_ptr<Enemy> enemy) {
	enemies_.push_back(std::move(enemy));
}

void LevelManager::SpawnEnemiesFromSpawnPoints() {
	lockedEnemy_ = nullptr;
	enemies_.clear();
	enemyRespawnTimers_.assign(enemySpawns_.size(), kNoRespawnTimer);

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
		if (enemySpawns_[spawnPointIndex].isInitialSpawn) {
			SpawnEnemyFromSpawnPoint(spawnPointIndex);
		}
	}
}

void LevelManager::SpawnEnemyFromSpawnPoint(size_t spawnPointIndex) {
	if (spawnPointIndex >= enemySpawns_.size()) {
		return;
	}

	const EnemySpawnData& spawnData = enemySpawns_[spawnPointIndex];
	auto enemy = std::make_unique<Enemy>();
	enemy->Initialize(spawnData.position);
	enemy->SetRotation(spawnData.rotation);
	
	// flightPath.IsValid() 等のチェックが必要だが EnemySpawnData の定義に依存する
	if (spawnData.flightPath.IsValid()) {
		enemy->SetFlightPath(spawnData.flightPath.points, spawnData.flightPath.loop, spawnData.flightPath.speed);
	}
	
	enemy->SetSpawnPointIndex(spawnPointIndex);
	enemies_.push_back(std::move(enemy));

	if (spawnPointIndex < enemyRespawnTimers_.size()) {
		enemyRespawnTimers_[spawnPointIndex] = kNoRespawnTimer;
	}
}

bool LevelManager::IsEnemySpawnPointActive(size_t spawnPointIndex) const {
	for (const auto& enemy : enemies_) {
		if (!enemy || enemy->GetSpawnPointIndex() != spawnPointIndex) {
			continue;
		}

		try {
			if (!enemy->IsDead()) {
				return true;
			}
		} catch (...) {
			continue;
		}
	}

	return false;
}

void LevelManager::ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames) {
	if (spawnPointIndex >= enemySpawns_.size() || spawnPointIndex >= enemyRespawnTimers_.size()) {
		return;
	}
	if (enemyRespawnTimers_[spawnPointIndex] != kNoRespawnTimer) {
		return;
	}
	if (IsEnemySpawnPointActive(spawnPointIndex)) {
		return;
	}

	enemyRespawnTimers_[spawnPointIndex] = delayFrames > 0 ? delayFrames : 1;
}

void LevelManager::TriggerEnemyReinforcements(const std::string& deadEnemyName) {
	if (deadEnemyName.empty()) {
		return;
	}

	std::vector<EnemyEvent> triggeredEvents = enemyEventManager_.GetEventsForTrigger(deadEnemyName);
	for (const auto& ev : triggeredEvents) {
		for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
			if (enemySpawns_[spawnPointIndex].name == ev.targetEnemyName) {
				ScheduleEnemySpawn(spawnPointIndex, ev.delayFrames);
				break;
			}
		}
	}

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
		const EnemySpawnData& spawnData = enemySpawns_[spawnPointIndex];
		if (spawnData.reinforcementTriggerName == deadEnemyName) {
			ScheduleEnemySpawn(spawnPointIndex, spawnData.reinforcementDelayFrames);
		}
	}
}

void LevelManager::UpdateEnemyRespawns() {
	for (size_t spawnPointIndex = 0; spawnPointIndex < enemyRespawnTimers_.size(); ++spawnPointIndex) {
		int& timer = enemyRespawnTimers_[spawnPointIndex];
		if (timer < 0) {
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
