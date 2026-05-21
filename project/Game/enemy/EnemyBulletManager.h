#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include <vector>
#include <memory>

class Player; // 前方宣言

class EnemyBulletManager {
public:
    void Initialize();
    // プレイヤーへのポインタを受け取って当たり判定を行う
    void Update(Player *player, std::vector<Vector3> &hitPositions);
    void Draw();

    // 弾を発射する
    void Shoot(const Vector3 &position, const Vector3 &velocity);

    // 弾1発のデータ構造
    struct Bullet {
        std::unique_ptr<Object3d> object;
        Vector3 position;
        Vector3 velocity;
        bool isDead = true; // デフォルトは非アクティブ
        int lifeTimer = 120; // 120フレーム（2秒）で自然消滅
    };

    // デバッグ表示用のゲッター
    const std::vector<Bullet>& GetBullets() const { return bullets_; }

private:
    static const size_t kMaxBullets = 50; // 弾の最大プール数
    std::vector<Bullet> bullets_;
};

