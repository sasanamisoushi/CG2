#pragma once

#include "engine/math/MyMath.h"
#include "Game/bullet/Missile.h"

#include <memory>
#include <vector>

class Camera;
class Object3d;
class Player;

// 弾倉・予備弾・リロード・補給アイテムを一括して管理する。
// GamePlayScene は武器発射と HUD から、このクラスの API だけを利用する。
class AmmoManager {
public:
    static constexpr int kNormalMagazineCapacity = 30;
    static constexpr int kNormalReserveCapacity = 120;
    static constexpr int kHomingMagazineCapacity = 8;
    static constexpr int kHomingReserveCapacity = 24;
    static constexpr int kReloadDurationFrames = 120;

    void Initialize();
    bool TryConsume(MissileType type);
    void UpdateReload(const Player* player);
    void RegisterSmallEnemyDefeat(const Vector3& position);
    void UpdatePickups(const Player* player);
    void DrawPickups(Camera* camera) const;

    int GetNormalMagazine() const { return normalMagazine_; }
    int GetNormalReserve() const { return normalReserve_; }
    int GetHomingMagazine() const { return homingMagazine_; }
    int GetHomingReserve() const { return homingReserve_; }
    bool IsNormalReloading() const { return isNormalReloading_; }
    bool IsHomingReloading() const { return isHomingReloading_; }
    int GetNormalReloadFrame() const { return normalReloadFrame_; }
    int GetHomingReloadFrame() const { return homingReloadFrame_; }

private:
    struct Pickup {
        Vector3 basePosition = { 0.0f, 0.0f, 0.0f };
        std::unique_ptr<Object3d> object;
        float phase = 0.0f;
    };

    void SpawnPickup(const Vector3& position);
    void FinishReload(int& magazine, int capacity, int& reserve);

    std::vector<Pickup> pickups_;
    int defeatedSmallEnemyCount_ = 0;
    int normalMagazine_ = kNormalMagazineCapacity;
    int normalReserve_ = 90;
    int homingMagazine_ = kHomingMagazineCapacity;
    int homingReserve_ = 16;
    bool isNormalReloading_ = false;
    bool isHomingReloading_ = false;
    int normalReloadFrame_ = 0;
    int homingReloadFrame_ = 0;
};
