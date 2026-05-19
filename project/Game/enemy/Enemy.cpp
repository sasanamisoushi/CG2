#include "Enemy.h"
#include "3D/Object3dCommon.h"
#include "engine/Math/MyMath.h"
#include "Game/enemy/EnemyBulletManager.h"

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
    object_->SetScale({ 1.0f, 1.0f, 1.0f });

    position_ = position;
}

void Enemy::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {

    if (isDead_) return; // 死んでいたら何もしない

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

    // 今は動かないので、座標をセットして更新するだけ
    object_->SetTranslate(position_);
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
