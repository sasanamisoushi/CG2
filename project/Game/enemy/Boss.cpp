#include "Boss.h"

void Boss::Initialize(const Vector3 &position) {
    // 基底クラス(Enemy)の初期化を呼ぶ
    Enemy::Initialize(position);

    // ボスとしての設定を上書きする
    SetIsBoss(true);
    
    // 例：HPを通常の10倍にする
    hp_ = 20;

    // 例：ボスのサイズを大きくする
    SetScale({ 3.0f, 3.0f, 3.0f });
}

void Boss::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    // まずは基底クラスのUpdateを呼んで基本的な挙動をさせる
    Enemy::Update(playerPos, bulletManager, obstacles);

    // ここにボス独自の攻撃パターンなどを追加できる
}
