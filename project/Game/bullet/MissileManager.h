#pragma once
#include "Game/bullet/Missile.h"
#include "engine/Camera/Camera.h"
#include <list>
#include <memory>

class Enemy;

class MissileManager {
public:
    // 初期化（リストを空にするなど）
    void Initialize();

    // 全ミサイルの更新と、死んだミサイルの削除
    void Update(Camera *camera, std::list<std::unique_ptr<Enemy>> &enemies);

    // 全ミサイルの描画
    void Draw();

    // ミサイルを発射する（GamePlaySceneから呼ばれる）
    void Shoot(const Vector3 &position, const Vector3 &velocity, MissileType type);

private:
    // ミサイルの実体を管理するリスト
    std::list<std::unique_ptr<Missile>> missiles_;
};

