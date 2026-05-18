#pragma once
#include "engine/Particle/ParticleManager.h"
#include "engine/Particle/ParticleEmitter.h"
#include "engine/math/MyMath.h"
#include <list>
#include <memory>
#include <vector>

class ExplosionManager {
public:
    // 初期化（ParticleManagerのポインタを受け取っておく）
    void Initialize(ParticleManager *particleManager);

    // 毎フレームの更新（寿命のカウントダウンと削除）
    void Update();

    // 報告された座標リストを受け取り、爆発を一気に発生させる
    void CreateExplosions(const std::vector<Vector3> &hitPositions);

private:
    // 爆発を管理する構造体
    struct Explosion {
        std::unique_ptr<ParticleEmitter> emitter;
        int timer;
    };

    std::list<Explosion> explosions_;
    ParticleManager *particleManager_ = nullptr; // エミッター生成時に必要な大元
};

