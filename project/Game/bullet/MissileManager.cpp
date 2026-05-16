#include "MissileManager.h"

void MissileManager::Initialize() {
    missiles_.clear();
}

void MissileManager::Update(Camera *camera) {
    // リスト内の全ミサイルを更新し、寿命が尽きたら削除する
    for (auto it = missiles_.begin(); it != missiles_.end(); ) {
        (*it)->Update(camera);

        if ((*it)->IsDead()) {
            it = missiles_.erase(it); // 削除
        } else {
            ++it;
        }
    }
}

void MissileManager::Draw() {
    for (const auto &missile : missiles_) {
        missile->Draw();
    }
}

void MissileManager::Shoot(const Vector3 &position, const Vector3 &velocity, MissileType type) {
    // 新しいミサイルを生み出してリストに追加する
    auto newMissile = std::make_unique<Missile>();
    newMissile->Initialize(position, velocity, type);

    missiles_.push_back(std::move(newMissile));
}