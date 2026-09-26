#include "AmmoManager.h"

#include "3D/Object3d.h"
#include "3D/Object3dCommon.h"
#include "engine/Input/Input.h"
#include "Game/Player/Player.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kKillsPerPickup = 5;
constexpr int kPickupNormalAmmo = 60;
constexpr int kPickupHomingAmmo = 12;
}

void AmmoManager::Initialize() {
    normalMagazine_ = kNormalMagazineCapacity;
    normalReserve_ = 90;
    homingMagazine_ = kHomingMagazineCapacity;
    homingReserve_ = 16;
    isNormalReloading_ = false;
    isHomingReloading_ = false;
    normalReloadFrame_ = 0;
    homingReloadFrame_ = 0;
    defeatedSmallEnemyCount_ = 0;
    pickups_.clear();
}

bool AmmoManager::TryConsume(MissileType type) {
    if ((type == MissileType::Normal && isNormalReloading_) ||
        (type == MissileType::MissileWithTrail && isHomingReloading_)) {
        return false;
    }
    int& magazine = (type == MissileType::Normal) ? normalMagazine_ : homingMagazine_;
    if (magazine <= 0) return false;
    --magazine;
    return true;
}

void AmmoManager::UpdateReload(const Player* player) {
    if (!player || player->IsDead()) return;
    Input* input = Input::GetInstance();
    if (!isNormalReloading_ && input->TriggerAction(PlayerAction::ReloadNormal) &&
        normalMagazine_ < kNormalMagazineCapacity && normalReserve_ > 0) {
        isNormalReloading_ = true;
        normalReloadFrame_ = 0;
    }
    if (!isHomingReloading_ && input->TriggerAction(PlayerAction::ReloadHoming) &&
        homingMagazine_ < kHomingMagazineCapacity && homingReserve_ > 0) {
        isHomingReloading_ = true;
        homingReloadFrame_ = 0;
    }
    if (isNormalReloading_ && ++normalReloadFrame_ >= kReloadDurationFrames) {
        FinishReload(normalMagazine_, kNormalMagazineCapacity, normalReserve_);
        isNormalReloading_ = false;
        normalReloadFrame_ = 0;
    }
    if (isHomingReloading_ && ++homingReloadFrame_ >= kReloadDurationFrames) {
        FinishReload(homingMagazine_, kHomingMagazineCapacity, homingReserve_);
        isHomingReloading_ = false;
        homingReloadFrame_ = 0;
    }
}

void AmmoManager::RegisterSmallEnemyDefeat(const Vector3& position) {
    if (++defeatedSmallEnemyCount_ % kKillsPerPickup == 0) SpawnPickup(position);
}

void AmmoManager::UpdatePickups(const Player* player) {
    if (!player) return;
    const Vector3 playerPosition = player->GetPosition();
    for (auto it = pickups_.begin(); it != pickups_.end();) {
        it->phase += 0.05f;
        Vector3 displayPosition = it->basePosition;
        displayPosition.y += std::sin(it->phase) * 0.5f;
        it->object->SetTranslate(displayPosition);
        it->object->SetRotate({ 0.0f, it->phase, 0.0f });
        it->object->Update();
        const float dx = displayPosition.x - playerPosition.x;
        const float dy = displayPosition.y - playerPosition.y;
        const float dz = displayPosition.z - playerPosition.z;
        if (dx * dx + dy * dy + dz * dz <= 9.0f) {
            normalReserve_ = (std::min)(kNormalReserveCapacity, normalReserve_ + kPickupNormalAmmo);
            homingReserve_ = (std::min)(kHomingReserveCapacity, homingReserve_ + kPickupHomingAmmo);
            it = pickups_.erase(it);
        } else {
            ++it;
        }
    }
}

void AmmoManager::DrawPickups(Camera*) const {
    for (const Pickup& pickup : pickups_) {
        if (pickup.object) {
            pickup.object->Draw();
            Object3dCommon::GetInstance()->SetCommonDrawSettings();
        }
    }
}

void AmmoManager::SpawnPickup(const Vector3& position) {
    Pickup pickup;
    pickup.basePosition = { position.x, position.y + 2.0f, position.z };
    pickup.phase = static_cast<float>(pickups_.size()) * 0.8f;
    pickup.object = std::make_unique<Object3d>();
    pickup.object->Initialize(Object3dCommon::GetInstance());
    pickup.object->SetModel("AmmoPickupSphere");
    pickup.object->SetScale({ 1.2f, 1.2f, 1.2f });
    pickup.object->SetTranslate(pickup.basePosition);
    pickup.object->Update();
    pickups_.push_back(std::move(pickup));
}

void AmmoManager::FinishReload(int& magazine, int capacity, int& reserve) {
    const int required = capacity - magazine;
    const int loaded = (std::min)(required, reserve);
    magazine += loaded;
    reserve -= loaded;
}
