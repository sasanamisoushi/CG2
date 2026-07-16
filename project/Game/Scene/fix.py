import sys

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # We will find ScheduleEnemySpawn
    idx1 = content.find('void GamePlayScene::ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames) {')
    if idx1 == -1: return 'Not found 1'
    idx2 = content.find('void GamePlayScene::UpdateEnemyCollision() {', idx1)
    if idx2 == -1: return 'Not found 2'
    
    new_text = '''void GamePlayScene::ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames) {
	if (spawnPointIndex >= enemySpawns_.size()) {
		return;
	}
	if (enemyRespawnTimers_.size() < enemySpawns_.size()) {
		enemyRespawnTimers_.resize(enemySpawns_.size(), kNoEnemyRespawnTimer);
	}
	if (enemyRespawnTimers_[spawnPointIndex] != kNoEnemyRespawnTimer || IsEnemySpawnPointActive(spawnPointIndex)) {
		return;
	}
	enemyRespawnTimers_[spawnPointIndex] = delayFrames > 0 ? delayFrames : 1;
}

void GamePlayScene::TriggerEnemyReinforcements(const std::string &deadEnemyName) {
	if (deadEnemyName.empty()) {
		return;
	}

	const std::vector<EnemyEvent> events = enemyEventManager_.GetEventsForTrigger(deadEnemyName);
	for (const EnemyEvent &event : events) {
		for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
			if (enemySpawns_[spawnPointIndex].name == event.targetEnemyName) {
				ScheduleEnemySpawn(spawnPointIndex, event.delayFrames);
				break;
			}
		}
	}

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
		EnemySpawnData &spawnData = enemySpawns_[spawnPointIndex];
		if (!spawnData.remainingReinforcementTriggers.empty()) {
			auto it = std::find(spawnData.remainingReinforcementTriggers.begin(), spawnData.remainingReinforcementTriggers.end(), deadEnemyName);
			if (it != spawnData.remainingReinforcementTriggers.end()) {
				spawnData.remainingReinforcementTriggers.erase(it);
				if (spawnData.remainingReinforcementTriggers.empty()) {
					ScheduleEnemySpawn(spawnPointIndex, spawnData.reinforcementDelayFrames);
				}
			}
		}
	}
}

void GamePlayScene::UpdateEnemyRespawns() {
	if (enemyRespawnTimers_.size() < enemySpawns_.size()) {
		enemyRespawnTimers_.resize(enemySpawns_.size(), kNoEnemyRespawnTimer);
	}

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemyRespawnTimers_.size(); ++spawnPointIndex) {
		int &timer = enemyRespawnTimers_[spawnPointIndex];
		if (timer == kNoEnemyRespawnTimer) {
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

bool GamePlayScene::HasPendingEnemySpawns() const {
	for (int timer : enemyRespawnTimers_) {
		if (timer != kNoEnemyRespawnTimer) {
			return true;
		}
	}
	return false;
}

'''
    
    final_content = content[:idx1] + new_text + content[idx2:]
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(final_content)
    return 'Success'

print(fix_file('c:\\\\Users\\\\k024g\\\\OneDrive\\\\デスクトップ\\\\2年\\\\2年前期\\\\CG2\\\\CG2\\\\project\\\\Game\\\\Scene\\\\GamePlayScene.cpp'))
