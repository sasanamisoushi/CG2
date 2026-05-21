#include "Enemy.h"
#include "3D/Object3dCommon.h"
#include "engine/Math/MyMath.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"

void Enemy::Initialize(const Vector3 &position) {
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dCommon::GetInstance());

    // 自機と同じBoxモデルを使い回す
    object_->SetModel("EnemyBox");

    // 敵っぽく赤色にする
    if (object_->GetModel()) {
        object_->GetModel()->SetColor({ 0.8f, 0.1f, 0.1f, 1.0f });
    }

    // 的として当てやすいように自機より少し大きくする
    scale_ = { 1.0f, 1.0f, 1.0f };
    object_->SetScale(scale_);

    position_ = position;
}

void Enemy::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {

    if (isDead_) return; // 死んでいたら何もしない

    // 1. AI思考と移動
    UpdateAI(playerPos, bulletManager);

    // 2. 当たり判定（押し出し）
    CheckCollision(obstacles);

    // 今は動かないので、座標をセットして更新するだけ
    object_->SetTranslate(position_);
    // モデルに回転を適用する！
    object_->SetRotate(rotation_);
    object_->SetScale(scale_);
    object_->Update();
}

void Enemy::Draw() {
    if (object_) {
        object_->Draw();
    }
}

// 当たった時の処理
void Enemy::OnCollision() {
    isDead_ = true;
}

void Enemy::UpdateAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {

    // ==========================================
    // 1. プレイヤーまでの距離と方向を計算
    // ==========================================
    Vector3 myPos = GetPosition(); // 今の自分の座標

    // 敵からプレイヤーへのベクトル（向き）
    Vector3 dir = {
        playerPos.x - myPos.x,
        playerPos.y - myPos.y,
        playerPos.z - myPos.z
    };

    // 距離の2乗を計算（ルートの計算は重いので、2乗のまま比べるのがゲームの基本です）
    float distSq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    float attackRadius = 30.0f; // 攻撃を開始する距離（30m）

    // ==========================================
    // 2. 状態（ステート）ごとの行動
    // ==========================================
    switch (state_) {

    case EnemyState::Approach:
        // もし攻撃範囲に入ったら、状態を「攻撃」に切り替える！
        if (distSq <= attackRadius * attackRadius) {
            state_ = EnemyState::Attack;
            attackTimer_ = 0; // タイマーをリセット
        } else {
            // 遠い場合はプレイヤーに向かって移動
            float dist = std::sqrt(distSq); // 実際の距離
            if (dist > 0.0f) {
                dir.x /= dist; // 方向ベクトルを正規化（長さを1にする）
                dir.y /= dist;
                dir.z /= dist;
            }

            float speed = 0.2f; // 敵の移動スピード
            position_.x += dir.x * speed;
            position_.y += dir.y * speed;
            position_.z += dir.z * speed;
        }
        break;

    case EnemyState::Attack:
        if (distSq > attackRadius * attackRadius) {
            state_ = EnemyState::Approach;
        } else {
            attackTimer_++;
            if (attackTimer_ >= 60) {
                // position_.y += 2.0f; // 攻撃の合図のジャンプ (下降処理がないため、戻り値がなく永久に上昇してしまうバグの原因になっていたのでコメントアウト)

                // ==========================================
                // 💥 弾を撃つ処理を追加！
                // ==========================================
                if (bulletManager) {
                    float dist = std::sqrt(distSq);
                    Vector3 velocity = { 0, 0, 0 };

                    // プレイヤーの方向へ向かう速度ベクトルを作る
                    if (dist > 0.0f) {
                        float bulletSpeed = 1.0f; // 弾の飛ぶ速さ
                        velocity.x = (dir.x / dist) * bulletSpeed;
                        velocity.y = (dir.y / dist) * bulletSpeed;
                        velocity.z = (dir.z / dist) * bulletSpeed;
                    }

                    // マネージャーに発射を依頼
                    bulletManager->Shoot(position_, velocity);
                }

                attackTimer_ = 0;
            }
        }
        break;
    }
}


void Enemy::CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    // =========================================================
    //    敵と障害物の当たり判定
    // =========================================================
    // 敵のAABB half-extents (EnemyBoxモデルは頂点±1 × スケール1.0 = extents 1.0)
    Vector3 enemyHalf = GetWorldHalfExtents();

    for (const auto &obstacle : obstacles) {
        Vector3 obsPos = obstacle->GetPosition();
        Vector3 obsRot = obstacle->GetRotation();
        // モデルの実際のバウンディングボックス半径 × Blenderスケール
        Vector3 obsHalf = obstacle->GetWorldHalfExtents();

        Matrix4x4 rotMat = MyMath::Multiply(MyMath::Multiply(MyMath::MakeRoteXMatrix(obsRot.x), MyMath::MakeRotateYMatrix(obsRot.y)), MyMath::MakeRotateZMatrix(obsRot.z));
        Matrix4x4 invRotMat = MyMath::Transpose(rotMat);

        Vector3 diff = { position_.x - obsPos.x, position_.y - obsPos.y, position_.z - obsPos.z };
        Vector3 localDiff = MyMath::Transform(diff, invRotMat);

        float dx = localDiff.x;
        float dy = localDiff.y;
        float dz = localDiff.z;

        float halfWidth = enemyHalf.x + obsHalf.x;
        float halfHeight = enemyHalf.y + obsHalf.y;
        float halfDepth = enemyHalf.z + obsHalf.z;

        float overlapX = halfWidth - std::abs(dx);
        float overlapY = halfHeight - std::abs(dy);
        float overlapZ = halfDepth - std::abs(dz);

        if (overlapX > 0.0f && overlapY > 0.0f && overlapZ > 0.0f) {
            Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

            if (overlapX < overlapY && overlapX < overlapZ) {
                localPushOut.x = (dx > 0.0f) ? overlapX : -overlapX;
            } else if (overlapY < overlapX && overlapY < overlapZ) {
                localPushOut.y = (dy > 0.0f) ? overlapY : -overlapY;
            } else {
                localPushOut.z = (dz > 0.0f) ? overlapZ : -overlapZ;
            }

            Vector3 worldPushOut = MyMath::Transform(localPushOut, rotMat);
            position_.x += worldPushOut.x;
            position_.y += worldPushOut.y;
            position_.z += worldPushOut.z;
        }
    }
}
