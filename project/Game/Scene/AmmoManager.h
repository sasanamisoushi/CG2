#pragma once

#include "Game/bullet/Missile.h"

#include <string>

// 弾倉と、空になった時の自動補充を管理する。
// GamePlayScene は武器発射と HUD から、このクラスの API だけを利用する。
class AmmoManager {
public:
    static constexpr int kDefaultNormalMagazineCapacity = 30;
    static constexpr int kDefaultHomingMagazineCapacity = 8;
    static constexpr int kReloadDurationFrames = 120;

    void Initialize();
    bool TryConsume(MissileType type);
    void UpdateAutoReload();
    bool SaveSettings(const std::string& filePath) const;
    bool LoadSettings(const std::string& filePath);
    void SetMagazineCapacities(int normalCapacity, int homingCapacity);
    void ReloadAll();

    int GetNormalMagazine() const { return normalMagazine_; }
    int GetHomingMagazine() const { return homingMagazine_; }
    int GetNormalMagazineCapacity() const { return normalMagazineCapacity_; }
    int GetHomingMagazineCapacity() const { return homingMagazineCapacity_; }
    bool IsNormalReloading() const { return isNormalReloading_; }
    bool IsHomingReloading() const { return isHomingReloading_; }
    int GetNormalReloadFrame() const { return normalReloadFrame_; }
    int GetHomingReloadFrame() const { return homingReloadFrame_; }

private:
    int normalMagazineCapacity_ = kDefaultNormalMagazineCapacity;
    int homingMagazineCapacity_ = kDefaultHomingMagazineCapacity;
    int normalMagazine_ = kDefaultNormalMagazineCapacity;
    int homingMagazine_ = kDefaultHomingMagazineCapacity;
    bool isNormalReloading_ = false;
    bool isHomingReloading_ = false;
    int normalReloadFrame_ = 0;
    int homingReloadFrame_ = 0;
};
