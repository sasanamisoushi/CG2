#pragma once
#include "Game/bullet/Missile.h"
#include "engine/Particle/ParticleEmitter.h"
#include "engine/Camera/Camera.h"
#include <list>
#include <memory>
#include <vector>

class Enemy;
class Obstacle; // 追加
#pragma once
#include "Game/bullet/Missile.h"
#include "engine/Particle/ParticleEmitter.h"
#include "engine/Camera/Camera.h"
#include <list>
#include <memory>
#include <vector>

class Enemy;
class Obstacle; // 追加

class MissileManager {
public:
    // 初期化（リストを空にするなど）
    void Initialize();

    // 全ミサイルの更新と、死んだミサイルの削除
    void Update(Camera *camera, std::list<std::unique_ptr<Enemy>> &enemies, const std::list<std::unique_ptr<Obstacle>> &obstacles, std::vector<Vector3> &hitPositions, Enemy *preferredTarget = nullptr);

    void UpdateModels(Camera *camera = nullptr);

    // 全ミサイルの描画
    void Draw();

    // ミサイルを発射する（GamePlaySceneから呼ばれる）
    void Shoot(const Vector3 &position, const Vector3 &velocity, MissileType type, const MissileTuning &tuning);

    // デバッグ表示用のゲッター
    const std::list<std::unique_ptr<Missile>>& GetMissiles() const { return missiles_; }

private:
    // ミサイルの実体を管理するリスト
    std::list<std::unique_ptr<Missile>> missiles_;
};

