#include "LockOnManager.h"
#include "MissilePresetManager.h"
#include "GamePlayScene.h"
#include "GamePlaySceneHelpers.h"
#include "Game/enemy/JammerEnemy.h"
#include <externals/imgui/imgui.h>
#include <fstream>
#include "engine/Input/Input.h"
#include "engine/math/MyMath.h"

LockOnManager::LockOnManager(GamePlayScene* scene) : scene_(scene) {}

void LockOnManager::UpdateLockOn(Camera *activeCamera, bool shouldUpdateGame) {
	Input *input = Input::GetInstance();
	if (!input) return;
	const bool canUseKeyboardInput = !IsImGuiKeyboardCaptureActive();

	if (canUseKeyboardInput && input->TriggerKey(DIK_TAB)) {
		// TABをトグル操作にする。ロック中なら解除し、未ロックなら対象を探す。
		if (scene_->lockedEnemy_ && IsLockedEnemyAlive()) {
			scene_->lockedEnemy_ = nullptr;
		} else {
			scene_->lockedEnemy_ = FindLockOnTarget(activeCamera);
		}
		scene_->aimAssistEnemy_ = nullptr;
		scene_->isCinematicLockOnCameraInitialized_ = false;
	}

	if (!IsLockedEnemyAlive()) {
		scene_->lockedEnemy_ = nullptr;
		scene_->isCinematicLockOnCameraInitialized_ = false;
	}

	scene_->aimAssistEnemy_ = nullptr;
	if (!scene_->lockedEnemy_ && shouldUpdateGame) {
		scene_->aimAssistEnemy_ = FindAimAssistTarget(activeCamera);
	}

	if (!shouldUpdateGame) {
		return;
	}

	if (canUseKeyboardInput && input->TriggerKey(DIK_X)) {
		scene_->lockedEnemy_ = nullptr;
		scene_->aimAssistEnemy_ = nullptr;
		scene_->isCinematicLockOnCameraInitialized_ = false;
		return;
	}

	if (scene_->lockedEnemy_) {
		scene_->lockedEnemy_->StartChasingPlayer();
	}
}

Enemy *LockOnManager::FindLockOnTarget(Camera *activeCamera) const {
	if (!scene_->player_ || !activeCamera || scene_->player_->IsDead()) {
		return nullptr;
	}

	const Vector3 playerPosition = scene_->player_->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(scene_->player_->GetForwardVector());
	Enemy *nearestAliveEnemy = nullptr;
	float nearestAliveDistSq = (std::numeric_limits<float>::max)();
	Enemy *bestFrontEnemy = nullptr;
	float bestFrontScore = (std::numeric_limits<float>::max)();
	Enemy *bestScreenEnemy = nullptr;
	float bestScreenScore = (std::numeric_limits<float>::max)();

	PlayerModeParams p = scene_->player_->GetModeParams(scene_->player_->GetCurrentMode());
	float maxDistSq = p.maxLockOnDistance * p.maxLockOnDistance;
	
	const bool isJammed = IsPlayerJammed(activeCamera);
	const float maxJammedDistanceSq = maxDistSq * 0.04f;

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	const bool hasOverlayBounds = activeCamera && GetOverlayBounds(minX, minY, maxX, maxY);
	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;

	for (const auto &enemy : scene_->enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);

		if (isJammed && distSq > maxJammedDistanceSq) {
			continue;
		}

		if (distSq < nearestAliveDistSq) {
			nearestAliveDistSq = distSq;
			nearestAliveEnemy = enemy.get();
		}

		const Vector3 direction = MyMath::Normalize(toEnemy);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot >= p.lockOnAngleDot) {
			const float frontScore = distSq * 0.01f - forwardDot * 1000.0f;
			if (frontScore < bestFrontScore) {
				bestFrontScore = frontScore;
				bestFrontEnemy = enemy.get();
			}
		}

		if (hasOverlayBounds && screenWidth > 0.0f && screenHeight > 0.0f && forwardDot >= p.lockOnAngleDot - 0.2f) {
			Vector3 targetPosition = enemy->GetPosition();
			float collisionRadius = 1.0f;
			try {
				collisionRadius = enemy->GetCollisionRadius();
			} catch (...) {}
			targetPosition.y += collisionRadius * 0.3f;
			Vector3 screenPosition = MyMath::WorldToScreen(
				targetPosition,
				activeCamera->GetViewProjectionMatrix(),
				screenWidth,
				screenHeight);

			if (screenPosition.z >= 0.0f && screenPosition.z <= 1.0f &&
				screenPosition.x >= 0.0f && screenPosition.x <= screenWidth &&
				screenPosition.y >= 0.0f && screenPosition.y <= screenHeight) {
				const float normalizedX = (screenPosition.x - screenWidth * 0.5f) / (screenWidth * 0.5f);
				const float normalizedY = (screenPosition.y - screenHeight * 0.5f) / (screenHeight * 0.5f);
				const float centerScore = normalizedX * normalizedX + normalizedY * normalizedY;
				const float screenScore = centerScore * 10000.0f + distSq * 0.002f - forwardDot * 150.0f;
				if (screenScore < bestScreenScore) {
					bestScreenScore = screenScore;
					bestScreenEnemy = enemy.get();
				}
			}
		}
	}

	if (bestScreenEnemy) {
		return bestScreenEnemy;
	}
	if (bestFrontEnemy) {
		return bestFrontEnemy;
	}
	return nearestAliveEnemy;
}

bool LockOnManager::IsLockedEnemyAlive() const {
	if (!scene_->lockedEnemy_) {
		return false;
	}

	for (const auto &enemy : scene_->enemies_) {
		if (enemy.get() == scene_->lockedEnemy_) {
			try {
				if (!enemy->IsDead()) {
					return true;
				}
			} catch (...) {
				return false;
			}
		}
	}

	return false;
}

bool LockOnManager::IsPlayerJammed(Camera* activeCamera) const {
	if (!scene_->player_ || scene_->player_->IsDead()) return false;
	const Vector3 playerPos = scene_->player_->GetPosition();
	for (const auto& enemy : scene_->enemies_) {
		if (enemy && !enemy->IsDead()) {
			if (auto jammer = dynamic_cast<JammerEnemy*>(enemy.get())) {
				float distSq = LengthSqVector3(SubtractVector3(playerPos, jammer->GetPosition()));
				float jammerRadius = jammer->GetJammingRadius();
				if (distSq <= jammerRadius * jammerRadius) {
					return true;
				}
			}
		}
	}
	return false;
}

Enemy *LockOnManager::FindAimAssistTarget(Camera *activeCamera) const {
	if (!scene_->player_ || !activeCamera || scene_->player_->IsDead()) {
		return nullptr;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
		return nullptr;
	}

	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
		return nullptr;
	}

	PlayerModeParams p = scene_->player_->GetModeParams(scene_->player_->GetCurrentMode());
	const Vector3 playerPosition = scene_->player_->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(scene_->player_->GetForwardVector());
	float maxDistanceSq = p.maxLockOnDistance * p.maxLockOnDistance;
	if (IsPlayerJammed(activeCamera)) {
		maxDistanceSq *= 0.04f; // 距離を20%に制限 (0.2 * 0.2 = 0.04)
	}
	const float centerX = screenWidth * 0.5f;
	const float centerY = screenHeight * 0.5f;
	Enemy *bestTarget = nullptr;
	float bestScore = (std::numeric_limits<float>::max)();

	for (const auto &enemy : scene_->enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);
		if (distSq > maxDistanceSq) {
			continue;
		}

		const Vector3 direction = MyMath::Normalize(toEnemy);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot < p.lockOnAngleDot) {
			continue;
		}

		Vector3 targetPosition = enemy->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = enemy->GetCollisionRadius();
		} catch (...) {}
		targetPosition.y += collisionRadius * 0.3f;

		Vector3 screenPosition = MyMath::WorldToScreen(
			targetPosition,
			activeCamera->GetViewProjectionMatrix(),
			screenWidth,
			screenHeight);

		if (screenPosition.z < 0.0f || screenPosition.z > 1.0f ||
			screenPosition.x < 0.0f || screenPosition.x > screenWidth ||
			screenPosition.y < 0.0f || screenPosition.y > screenHeight) {
			continue;
		}

		const float dx = screenPosition.x - centerX;
		const float dy = screenPosition.y - centerY;
		const float screenDistanceSq = dx * dx + dy * dy;
		const float assistRadius = kAimAssistScreenRadius + collisionRadius * 12.0f;
		if (screenDistanceSq > assistRadius * assistRadius) {
			continue;
		}

		const float score = screenDistanceSq + distSq * 0.01f - forwardDot * 40.0f;
		if (score < bestScore) {
			bestScore = score;
			bestTarget = enemy.get();
		}
	}

	return bestTarget;
}

Enemy *LockOnManager::FindMultiLockTarget(Camera *activeCamera) const {
	if (!scene_->player_ || !activeCamera || scene_->player_->IsDead()) {
		return nullptr;
	}
	PlayerModeParams p = scene_->player_->GetModeParams(scene_->player_->GetCurrentMode());
	if (scene_->multiLockTargets_.size() >= p.maxMultiLock) {
		return nullptr;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
		return nullptr;
	}

	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
		return nullptr;
	}

	const Vector3 playerPosition = scene_->player_->GetPosition();
	const Vector3 playerForward = NormalizeOrVector3(scene_->player_->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	float maxDistanceSq = p.maxLockOnDistance * p.maxLockOnDistance;
	if (IsPlayerJammed(activeCamera)) {
		maxDistanceSq *= 0.04f; // 距離を20%に制限
	}
	const float centerX = screenWidth * 0.5f;
	const float centerY = screenHeight * 0.5f;
	Enemy *bestTarget = nullptr;
	float bestScore = (std::numeric_limits<float>::max)();

	for (const auto &enemy : scene_->enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }

		int lockCount = 0;
		for (Enemy *target : scene_->multiLockTargets_) {
			if (target == enemy.get()) {
				lockCount++;
			}
		}
		// 1体の敵に集中ロック可能にするため、alreadyLocked による continue は廃止する。
		// 代わりに、同じ敵に偏りすぎないよう、ロック済み回数に応じてスコアペナルティを与える。

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);
		if (distSq > maxDistanceSq) {
			continue;
		}

		const Vector3 direction = NormalizeOrVector3(toEnemy, playerForward);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot < p.lockOnAngleDot) {
			continue;
		}

		Vector3 targetPosition = enemy->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = enemy->GetCollisionRadius();
		} catch (...) {}
		targetPosition.y += collisionRadius * 0.3f;

		Vector3 screenPosition = MyMath::WorldToScreen(
			targetPosition,
			activeCamera->GetViewProjectionMatrix(),
			screenWidth,
			screenHeight);

		if (screenPosition.z < 0.0f || screenPosition.z > 1.0f ||
			screenPosition.x < 0.0f || screenPosition.x > screenWidth ||
			screenPosition.y < 0.0f || screenPosition.y > screenHeight) {
			continue;
		}

		const float dx = screenPosition.x - centerX;
		const float dy = screenPosition.y - centerY;
		const float screenDistanceSq = dx * dx + dy * dy;
		const float lockRadius = kMultiLockScreenRadius + collisionRadius * 16.0f;
		if (screenDistanceSq > lockRadius * lockRadius) {
			continue;
		}

		// lockCount に応じてペナルティを加算（まだロックされていない敵がいればそちらを優先）
		const float score = screenDistanceSq + distSq * 0.006f - forwardDot * 80.0f + (lockCount * 20000.0f);
		if (score < bestScore) {
			bestScore = score;
			bestTarget = enemy.get();
		}
	}

	return bestTarget;
}

void LockOnManager::BeginMultiLock() {
	scene_->isMultiLockCharging_ = true;
	scene_->multiLockChargeFrames_ = 0;
	scene_->multiLockTargets_.clear();
}

void LockOnManager::PruneMultiLockTargets() {
	auto isTargetAlive = [this](Enemy *target) {
		if (!target) {
			return false;
		}
		for (const auto &enemy : scene_->enemies_) {
			if (enemy.get() == target) {
				try {
					return !enemy->IsDead();
				} catch (...) {
					return false;
				}
			}
		}
		return false;
	};

	scene_->multiLockTargets_.erase(
		std::remove_if(
			scene_->multiLockTargets_.begin(),
			scene_->multiLockTargets_.end(),
			[&](Enemy *target) { return !isTargetAlive(target); }),
		scene_->multiLockTargets_.end());
}

void LockOnManager::UpdateMultiLock(Camera *activeCamera) {
	if (!scene_->isMultiLockCharging_) {
		return;
	}

	PruneMultiLockTargets();
	PlayerModeParams p = scene_->player_->GetModeParams(scene_->player_->GetCurrentMode());
	int acquireInterval = scene_->isSongActive_ ? 1 : kMultiLockAcquireIntervalFrames;
	if (scene_->multiLockTargets_.size() < p.maxMultiLock &&
		(scene_->multiLockChargeFrames_ == 0 || scene_->multiLockChargeFrames_ % acquireInterval == 0)) {
		if (Enemy *target = FindMultiLockTarget(activeCamera)) {
			scene_->multiLockTargets_.push_back(target);
			target->StartChasingPlayer();
		}
	}

	++scene_->multiLockChargeFrames_;
}

void LockOnManager::FireMultiLockMissiles() {
	if (!scene_->isMultiLockCharging_) {
		return;
	}

	PruneMultiLockTargets();
	if (scene_->multiLockTargets_.empty()) {
		if (scene_->aimAssistEnemy_) {
			scene_->multiLockTargets_.push_back(scene_->aimAssistEnemy_);
		} else if (scene_->lockedEnemy_ && IsLockedEnemyAlive()) {
			scene_->multiLockTargets_.push_back(scene_->lockedEnemy_);
		}
	}

	if (scene_->multiLockTargets_.empty()) {
		scene_->missilePresetManager_->FirePlayerMissile(MissileType::MissileWithTrail);
	} else {
		const float spacing = 0.35f;
		const float center = (static_cast<float>(scene_->multiLockTargets_.size()) - 1.0f) * 0.5f;
		for (size_t index = 0; index < scene_->multiLockTargets_.size(); ++index) {
			const float horizontalOffset = (static_cast<float>(index) - center) * spacing;
			scene_->missilePresetManager_->FirePlayerMissile(MissileType::MissileWithTrail, scene_->multiLockTargets_[index], horizontalOffset);
		}
	}

	CancelMultiLock();
}

void LockOnManager::CancelMultiLock() {
	scene_->isMultiLockCharging_ = false;
	scene_->multiLockChargeFrames_ = 0;
	scene_->multiLockTargets_.clear();
}

