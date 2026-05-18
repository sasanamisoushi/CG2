#pragma once
#include "engine/Particle/ParticleManager.h"
#include "engine/Particle/ParticleEmitter.h"
#include "engine/math/MyMath.h"
#include <list>
#include <memory>
#include <vector>

class ExplosionManager {
public:
    struct ExplosionConfig {
        int count = 100;
        float color[4] = { 1.0f, 0.5f, 0.1f, 1.0f };
        float speed = 0.5f;
        float speedVariance = 0.2f;
        float scale = 0.1f;
        float scaleVariance = 0.05f;
        float lifeTimeMin = 0.5f;
        float lifeTimeMax = 1.0f;
        float posVariance = 0.1f;
    };

    // 初期化（ParticleManagerのポインタを受け取っておく）
    void Initialize(ParticleManager *particleManager);

    // 毎フレームの更新（寿命のカウントダウンと削除）
    void Update();

    // 報告された座標リストを受け取り、爆発を一気に発生させる
    void CreateExplosions(const std::vector<Vector3> &hitPositions);

    // 設定の取得
    ExplosionConfig& GetConfig() { return config_; }

    // JSON保存/読込
    void SaveToJson(const std::string &filepath);
    void LoadFromJson(const std::string &filepath);

private:
    // 爆発を管理する構造体
    struct Explosion {
        std::unique_ptr<ParticleEmitter> emitter;
        int timer;
    };

    std::list<Explosion> explosions_;
    ParticleManager *particleManager_ = nullptr; // エミッター生成時に必要な大元
    ExplosionConfig config_;
};

