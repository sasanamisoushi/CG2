#pragma once
#include "Enemy.h"

// ボス敵を表すクラス
class Boss : public Enemy {
public:
    Boss() = default;
    ~Boss() override = default;

    // 初期化をオーバーライドして、ボス用のHPやサイズを設定する
    void Initialize(const Vector3 &position) override;

    // 必要に応じてUpdateやDrawもオーバーライド可能
    void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) override;
};
