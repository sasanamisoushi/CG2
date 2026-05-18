#include "ExplosionManager.h"

void ExplosionManager::Initialize(ParticleManager *particleManager) {
    particleManager_ = particleManager;
    explosions_.clear();
}

void ExplosionManager::CreateExplosions(const std::vector<Vector3> &hitPositions) {
    if (!particleManager_) return;

    for (const Vector3 &pos : hitPositions) {
        Explosion exp;
        // "test" パーティクルを生成
        exp.emitter = std::make_unique<ParticleEmitter>("test", pos, particleManager_);
        
        exp.emitter->count = 30;           // 1回の発生で30個出す
        exp.emitter->frequency_ = 0.01f;    // 発生間隔を極限まで短くする（一気に出る）
        exp.emitter->radius = 2.0f;        // 半径2mの範囲に散らす


        exp.timer = 15; // 15フレームでエミッター消滅
        explosions_.push_back(std::move(exp));
    }
}

void ExplosionManager::Update() {
    for (auto it = explosions_.begin(); it != explosions_.end(); ) {
        it->emitter->Update();
        it->timer--;

        if (it->timer <= 0) {
            it = explosions_.erase(it);
        } else {
            ++it;
        }
    }
}