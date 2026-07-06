#pragma once

#include <string>
#include <vector>
#include "engine/Camera/Camera.h"
#include "Game/enemy/Enemy.h"
#include "Game/Player/Player.h"
#include "Game/bullet/Missile.h"
#include "externals/json.hpp"

class GamePlayScene;

class LockOnManager {
public:
	LockOnManager(GamePlayScene* scene);
	void UpdateLockOn(Camera *activeCamera, bool shouldUpdateGame);
	Enemy *FindLockOnTarget(Camera *activeCamera) const;
	bool IsLockedEnemyAlive() const;
	Enemy *FindAimAssistTarget(Camera *activeCamera) const;
	Enemy *FindMultiLockTarget(Camera *activeCamera) const;
	void BeginMultiLock();
	void PruneMultiLockTargets();
	void UpdateMultiLock(Camera *activeCamera);
	void FireMultiLockMissiles();
	void CancelMultiLock();

private:
	GamePlayScene* scene_;
};
