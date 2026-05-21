#include "EnemyBulletManager.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"

void EnemyBulletManager::Initialize() {
    bullets_.clear();
    bullets_.resize(kMaxBullets);
    for (auto &bullet : bullets_) {
        bullet.object = std::make_unique<Object3d>();
        bullet.object->Initialize(Object3dCommon::GetInstance());
        bullet.object->SetModel("EnemyBox"); // 自機や敵と同じモデルを使い回す
        bullet.object->SetScale({ 0.5f, 0.5f, 0.5f }); // 小さくする
        if (bullet.object->GetModel()) {
            bullet.object->GetModel()->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f }); // 赤色
        }
        bullet.isDead = true; // 最初は非アクティブに設定
    }
}

void EnemyBulletManager::Update(Player *player, std::vector<Vector3> &hitPositions) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) continue; // 非アクティブな弾はスキップ

        // 1. 移動処理
        bullet.position.x += bullet.velocity.x;
        bullet.position.y += bullet.velocity.y;
        bullet.position.z += bullet.velocity.z;

        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();

        // プレイヤーの座標を取得
        Vector3 playerPos = player->GetPosition();

        // 2. 寿命チェック
        bullet.lifeTimer--;
        if (bullet.lifeTimer <= 0) {
            bullet.isDead = true;
        }
        if (!player->IsDead()) {
            float dx = playerPos.x - bullet.position.x;
            float dy = playerPos.y - bullet.position.y;
            float dz = playerPos.z - bullet.position.z;
            float distSq = dx * dx + dy * dy + dz * dz;

            // 半径1m以内ならヒット
            if (distSq < 1.0f * 1.0f) {
                bullet.isDead = true; // 弾を消す

                // プレイヤーにダメージを与える（倒す）！
                //player->OnCollision();

                // 当たった場所（自機の座標）を爆発リストに報告！
                hitPositions.push_back(playerPos);
            }
        }
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
    for (auto &bullet : bullets_) {
        if (bullet.isDead) { // 使用可能な弾を検索
            bullet.position = position;
            bullet.velocity = velocity;
            bullet.lifeTimer = 120;
            bullet.isDead = false;

            bullet.object->SetTranslate(position);
            bullet.object->Update();
            break; // 発射したので終了
        }
    }
}
