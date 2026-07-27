#include "EnemyBulletManager.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"
#include "Game/obstacle/Obstacle.h" // 追加

namespace {
    constexpr float kEnemyBulletScale = 0.2f;
    constexpr float kEnemyBulletRadius = 0.2f;
}

void EnemyBulletManager::Initialize() {
    bullets_.clear();
    bullets_.resize(kMaxBullets);
    for (auto &bullet : bullets_) {
        bullet.object = std::make_unique<Object3d>();
        bullet.object->Initialize(Object3dCommon::GetInstance());
        bullet.object->SetModel("EnemyBox"); // 自機や敵と同じモデルを使い回し
        bullet.object->SetScale({ kEnemyBulletScale, kEnemyBulletScale, kEnemyBulletScale }); // 小さくする
        if (bullet.object->GetModel()) {
            bullet.object->GetModel()->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f }); // 赤色
        }
        bullet.isDead = true; // 最初は非アクティブに設定
    }
}

void EnemyBulletManager::Update(Player *player, std::vector<Vector3> &hitPositions, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) continue; // 非アクティブな弾はスキップ

        // 1. 移動処理
        bullet.position.x += bullet.velocity.x;
        bullet.position.y += bullet.velocity.y;
        bullet.position.z += bullet.velocity.z;

        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();

        // 2. 寿命チェック
        bullet.lifeTimer--;
        if (bullet.lifeTimer <= 0) {
            bullet.isDead = true;
            continue;
        }

        // 3. 障害物との当たり判定
        bool hitObstacle = false;
        Sphere bulletSphere;
        bulletSphere.center = bullet.position;
        bulletSphere.radius = bullet.collisionRadius;

        for (const auto& obstacle : obstacles) {
            if (obstacle->IsStageBounds()) {
                continue;
            }

            OBB obsOBB = obstacle->GetOBB();
            if (MyMath::IsCollision(bulletSphere, obsOBB)) {
                bullet.isDead = true;
                hitObstacle = true;
                break;
            }
        }

        if (hitObstacle) {
            continue; // 障害物に当たったら以降の判定はスキップ
        }

        if (!player->IsDead()) {
            OBB playerOBB = player->GetOBB();
            if (MyMath::IsCollision(bulletSphere, playerOBB)) {
                bullet.isDead = true; // 弾を消す

                // プレイヤーにダメージを与える
                player->TakeDamage(bullet.damage);

                if (!player->IsDead()) {
                    hitPositions.push_back(player->GetPosition());
                }
            }
        }
    }
}

void EnemyBulletManager::UpdateModels() {
    for (auto &bullet : bullets_) {
        if (bullet.isDead || !bullet.object) {
            continue;
        }

        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();
    }
}

void EnemyBulletManager::Draw() {
    for (auto &bullet : bullets_) {
        if (!bullet.isDead) { // アクティブな弾のみ描画
            bullet.object->Draw();
        }
    }
}

void EnemyBulletManager::Shoot(const Vector3 &position, const Vector3 &velocity) {
    ShootConfigured(position, velocity, { kEnemyBulletScale, kEnemyBulletScale, kEnemyBulletScale },
                    kEnemyBulletRadius, 1, 120, "EnemyBox");
}

void EnemyBulletManager::ShootHeavyCannon(const Vector3 &position, const Vector3 &velocity) {
    ShootConfigured(position, velocity, { 1.2f, 1.2f, 2.8f }, 1.2f, 2, 240, "BossCannon");
}

void EnemyBulletManager::ShootBeam(const Vector3 &position, const Vector3 &velocity) {
    ShootConfigured(position, velocity, { 3.5f, 3.5f, 14.0f }, 3.5f, 3, 150, "BossBeam");
}

void EnemyBulletManager::ShootConfigured(const Vector3 &position, const Vector3 &velocity, const Vector3 &scale,
                                         float collisionRadius, int damage, int lifeTimer, const char *modelName) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) { // 使用可能な弾を検索
            bullet.position = position;
            bullet.velocity = velocity;
            bullet.lifeTimer = lifeTimer;
            bullet.collisionRadius = collisionRadius;
            bullet.damage = damage;
            bullet.isDead = false;

            bullet.object->SetModel(modelName);
            bullet.object->SetScale(scale);
            bullet.object->SetTranslate(position);
            bullet.object->Update();
            break; // 発射したので終了
        }
    }
}

