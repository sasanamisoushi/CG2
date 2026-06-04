#include "Enemy.h"
#include "3D/Object3dCommon.h"
#include "engine/Math/MyMath.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"

namespace {
    constexpr float kAttackRadius = 120.0f;
    constexpr int kAttackInterval = 300;
    constexpr float kEnemyMoveSpeed = 0.2f;
    constexpr float kEnemyBulletSpeed = 0.6f;
}

void Enemy::Initialize(const Vector3 &position) {
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dCommon::GetInstance());

    // 閾ｪ讖溘→蜷後§Box繝｢繝・Ν繧剃ｽｿ縺・屓縺・
    object_->SetModel("EnemyBox");

    // 謨ｵ縺｣縺ｽ縺剰ｵ､濶ｲ縺ｫ縺吶ｋ
    if (object_->GetModel()) {
        object_->GetModel()->SetColor({ 0.8f, 0.1f, 0.1f, 1.0f });
    }

    // 繝励Ξ繧､繝､繝ｼ縺ｨ蜷後§螟ｧ縺阪＆縺ｫ縺吶ｋ
    scale_ = { 0.2f, 0.2f, 0.2f };
    object_->SetScale(scale_);

    position_ = position;
    rotation_ = { 0.0f, 0.0f, 0.0f };
    isDead_ = false;
    state_ = EnemyState::Approach;
    attackTimer_ = kAttackInterval;

    // リスポーン直後の描画でもBlenderで設定した地点に表示されるように同期する。
    object_->SetTranslate(position_);
    object_->SetRotate(rotation_);
    object_->Update();
}

void Enemy::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {

    if (isDead_) return; // 豁ｻ繧薙〒縺・◆繧我ｽ輔ｂ縺励↑縺・

    // 1. AI諤晁・→遘ｻ蜍・
    UpdateAI(playerPos, bulletManager);

    // 2. 蠖薙◆繧雁愛螳夲ｼ域款縺怜・縺暦ｼ・
    CheckCollision(obstacles);

    // 莉翫・蜍輔°縺ｪ縺・・縺ｧ縲∝ｺｧ讓吶ｒ繧ｻ繝・ヨ縺励※譖ｴ譁ｰ縺吶ｋ縺縺・
    object_->SetTranslate(position_);
    // 繝｢繝・Ν縺ｫ蝗櫁ｻ｢繧帝←逕ｨ縺吶ｋ・・
    object_->SetRotate(rotation_);
    object_->SetScale(scale_);
    object_->Update();
}

void Enemy::Draw() {
    if (object_) {
        object_->Draw();
    }
}

// 蠖薙◆縺｣縺滓凾縺ｮ蜃ｦ逅・
void Enemy::OnCollision() {
    isDead_ = true;
}

void Enemy::UpdateAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {
    Vector3 toPlayer = {
        playerPos.x - position_.x,
        playerPos.y - position_.y,
        playerPos.z - position_.z
    };

    float distSqToPlayer = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z;
    float distToPlayer = std::sqrt(distSqToPlayer);
    Vector3 directionToPlayer = { 0.0f, 0.0f, -1.0f };
    if (distToPlayer > 0.001f) {
        directionToPlayer.x = toPlayer.x / distToPlayer;
        directionToPlayer.y = toPlayer.y / distToPlayer;
        directionToPlayer.z = toPlayer.z / distToPlayer;
    }

    if (distSqToPlayer > kAttackRadius * kAttackRadius) {
        state_ = EnemyState::Approach;
        position_.x += directionToPlayer.x * kEnemyMoveSpeed;
        position_.y += directionToPlayer.y * kEnemyMoveSpeed;
        position_.z += directionToPlayer.z * kEnemyMoveSpeed;
        return;
    }

    state_ = EnemyState::Attack;
    attackTimer_++;
    if (attackTimer_ >= kAttackInterval && bulletManager) {
        Vector3 velocity = {
            directionToPlayer.x * kEnemyBulletSpeed,
            directionToPlayer.y * kEnemyBulletSpeed,
            directionToPlayer.z * kEnemyBulletSpeed
        };
        bulletManager->Shoot(position_, velocity);
        attackTimer_ = 0;
    }
}

void Enemy::CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    // =========================================================
    //    謨ｵ縺ｨ髫懷ｮｳ迚ｩ縺ｮ蠖薙◆繧雁愛螳・
    // =========================================================
    // 謨ｵ縺ｮAABB half-extents (EnemyBox繝｢繝・Ν縺ｯ鬆らせﾂｱ1 ﾃ・繧ｹ繧ｱ繝ｼ繝ｫ1.0 = extents 1.0)
    Vector3 enemyHalf = GetWorldHalfExtents();

    for (const auto &obstacle : obstacles) {
        Vector3 obsPos = obstacle->GetPosition();
        Vector3 obsRot = obstacle->GetRotation();
        // 繝｢繝・Ν縺ｮ螳滄圀縺ｮ繝舌え繝ｳ繝・ぅ繝ｳ繧ｰ繝懊ャ繧ｯ繧ｹ蜊雁ｾ・ﾃ・Blender繧ｹ繧ｱ繝ｼ繝ｫ
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

        // StageBounds は壁ではなくステージ全体の範囲なので、内側に留める。
        if (obstacle->IsStageBounds()) {
            Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

            if (dx > obsHalf.x - enemyHalf.x) {
                localPushOut.x = (obsHalf.x - enemyHalf.x) - dx;
            } else if (dx < -(obsHalf.x - enemyHalf.x)) {
                localPushOut.x = -(obsHalf.x - enemyHalf.x) - dx;
            }

            if (dy > obsHalf.y - enemyHalf.y) {
                localPushOut.y = (obsHalf.y - enemyHalf.y) - dy;
            } else if (dy < -(obsHalf.y - enemyHalf.y)) {
                localPushOut.y = -(obsHalf.y - enemyHalf.y) - dy;
            }

            if (dz > obsHalf.z - enemyHalf.z) {
                localPushOut.z = (obsHalf.z - enemyHalf.z) - dz;
            } else if (dz < -(obsHalf.z - enemyHalf.z)) {
                localPushOut.z = -(obsHalf.z - enemyHalf.z) - dz;
            }

            Vector3 worldPushOut = MyMath::Transform(localPushOut, rotMat);
            position_.x += worldPushOut.x;
            position_.y += worldPushOut.y;
            position_.z += worldPushOut.z;
            continue;
        }

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


