#include "Enemy.h"
#include "3D/Object3dCommon.h"
#include "engine/Math/MyMath.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kAttackRadius = 12.0f;
    constexpr float kBreakAwayRadius = 1.2f;
    constexpr float kRetreatStartRadius = 10.0f;
    constexpr int kRetreatCycleFrames = 300;
    constexpr int kRetreatStartFrame = 170;
    constexpr int kRetreatDurationFrames = 90;
    constexpr int kAttackInterval = 90;
    constexpr float kEnemyCruiseSpeed = 0.05f;
    constexpr float kEnemyBoostSpeed = 0.08f;
    constexpr float kEnemyTurnBlend = 0.045f;
    constexpr float kEnemyAttackTurnBlend = 0.065f;
    constexpr float kEnemyRetreatTurnBlend = 0.085f;
    constexpr float kEnemyAccelerationBlend = 0.035f;
    constexpr float kEnemyBankResponse = 0.12f;
    constexpr float kEnemyMaxBankAngle = 0.75f;
    constexpr float kEnemyBulletSpeed = 0.18f;
    constexpr float kPi = 3.1415926535f;

    float LengthSq(const Vector3 &v) {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    float Length(const Vector3 &v) {
        return std::sqrt(LengthSq(v));
    }

    Vector3 Add(const Vector3 &a, const Vector3 &b) {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    Vector3 Subtract(const Vector3 &a, const Vector3 &b) {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    Vector3 Scale(const Vector3 &v, float scalar) {
        return { v.x * scalar, v.y * scalar, v.z * scalar };
    }

    Vector3 NormalizeOr(const Vector3 &v, const Vector3 &fallback) {
        float length = Length(v);
        if (length <= 0.001f) {
            return fallback;
        }
        return Scale(v, 1.0f / length);
    }

    Vector3 ForwardFromRotation(const Vector3 &rotation) {
        Matrix4x4 rotMat = MyMath::Multiply(
            MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation.x), MyMath::MakeRotateYMatrix(rotation.y)),
            MyMath::MakeRotateZMatrix(rotation.z));
        return NormalizeOr(MyMath::Transform({ 0.0f, 0.0f, 1.0f }, rotMat), { 0.0f, 0.0f, 1.0f });
    }

    Vector3 ToEulerRotation(const Vector3 &forward, float bankAngle) {
        Vector3 normalizedForward = NormalizeOr(forward, { 0.0f, 0.0f, 1.0f });
        float clampedY = std::clamp(normalizedForward.y, -1.0f, 1.0f);
        return {
            -std::asin(clampedY),
            std::atan2(normalizedForward.x, normalizedForward.z),
            bankAngle
        };
    }

    int GetFlightSide(size_t spawnPointIndex, const Vector3 &position) {
        if (spawnPointIndex != Enemy::kNoSpawnPoint) {
            return (spawnPointIndex % 2 == 0) ? 1 : -1;
        }
        return (position.x >= 0.0f) ? 1 : -1;
    }

    int GetRetreatPhaseOffset(size_t spawnPointIndex, const Vector3 &position) {
        if (spawnPointIndex != Enemy::kNoSpawnPoint) {
            return static_cast<int>((spawnPointIndex * 73) % kRetreatCycleFrames);
        }
        return static_cast<int>(std::abs(position.x) * 37.0f) % kRetreatCycleFrames;
    }
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
    forward_ = ForwardFromRotation(rotation_);
    currentSpeed_ = kEnemyCruiseSpeed;
    velocity_ = Scale(forward_, currentSpeed_);
    bankAngle_ = 0.0f;
    flightTimer_ = 0;
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

void Enemy::SetRotation(const Vector3 &rotation) {
    rotation_ = rotation;
    bankAngle_ = rotation.z;
    forward_ = ForwardFromRotation(rotation_);
    currentSpeed_ = (currentSpeed_ > 0.0f) ? currentSpeed_ : kEnemyCruiseSpeed;
    velocity_ = Scale(forward_, currentSpeed_);

    if (object_) {
        object_->SetRotate(rotation_);
    }
}

void Enemy::UpdateAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {
    flightTimer_++;

    Vector3 toPlayer = Subtract(playerPos, position_);
    float distSqToPlayer = LengthSq(toPlayer);
    float distToPlayer = std::sqrt(distSqToPlayer);
    Vector3 directionToPlayer = NormalizeOr(toPlayer, forward_);

    bool isInAttackRange = distSqToPlayer <= kAttackRadius * kAttackRadius;
    state_ = isInAttackRange ? EnemyState::Attack : EnemyState::Approach;

    int flightSide = GetFlightSide(spawnPointIndex_, position_);
    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
    Vector3 sideDirection = MyMath::Cross(worldUp, directionToPlayer);
    if (LengthSq(sideDirection) <= 0.001f) {
        sideDirection = { static_cast<float>(flightSide), 0.0f, 0.0f };
    } else {
        sideDirection = Scale(NormalizeOr(sideDirection, { 1.0f, 0.0f, 0.0f }), static_cast<float>(flightSide));
    }

    int retreatPhase = (flightTimer_ + GetRetreatPhaseOffset(spawnPointIndex_, position_)) % kRetreatCycleFrames;
    bool isRetreating = isInAttackRange &&
        distSqToPlayer <= kRetreatStartRadius * kRetreatStartRadius &&
        retreatPhase >= kRetreatStartFrame &&
        retreatPhase < kRetreatStartFrame + kRetreatDurationFrames;

    float wave = std::sin(static_cast<float>(flightTimer_) * 0.035f + static_cast<float>(flightSide) * kPi * 0.5f);
    float desiredAltitude = playerPos.y + 7.0f + wave * 4.0f;
    float altitudeCorrection = std::clamp((desiredAltitude - position_.y) / 45.0f, -0.35f, 0.35f);

    Vector3 desiredDirection = directionToPlayer;
    float desiredSpeed = kEnemyBoostSpeed;
    float turnBlend = kEnemyTurnBlend;

    if (isRetreating) {
        desiredDirection = Add(
            Scale(directionToPlayer, -1.0f),
            Scale(sideDirection, 0.55f));
        desiredDirection.y += altitudeCorrection * 0.8f;

        desiredSpeed = kEnemyBoostSpeed;
        turnBlend = kEnemyRetreatTurnBlend;
    } else if (isInAttackRange) {
        float tangentWeight = (distToPlayer < kBreakAwayRadius) ? 1.25f : 0.75f;
        float noseWeight = (distToPlayer < kBreakAwayRadius) ? -0.15f : 0.65f;

        desiredDirection = Add(
            Scale(directionToPlayer, noseWeight),
            Scale(sideDirection, tangentWeight));
        desiredDirection.y += altitudeCorrection;

        desiredSpeed = (distToPlayer < kBreakAwayRadius) ? kEnemyBoostSpeed : kEnemyCruiseSpeed;
        turnBlend = kEnemyAttackTurnBlend;
    } else {
        desiredDirection.y += altitudeCorrection * 0.5f;
    }

    desiredDirection = NormalizeOr(desiredDirection, directionToPlayer);

    Vector3 oldForward = forward_;
    forward_ = NormalizeOr(Add(Scale(forward_, 1.0f - turnBlend), Scale(desiredDirection, turnBlend)), oldForward);

    float targetSpeed = desiredSpeed;
    currentSpeed_ += (targetSpeed - currentSpeed_) * kEnemyAccelerationBlend;
    velocity_ = Scale(forward_, currentSpeed_);

    position_.x += velocity_.x;
    position_.y += velocity_.y;
    position_.z += velocity_.z;

    float yawTurn = MyMath::Dot(MyMath::Cross(oldForward, forward_), worldUp);
    float targetBankAngle = std::clamp(-yawTurn * 18.0f, -kEnemyMaxBankAngle, kEnemyMaxBankAngle);
    bankAngle_ += (targetBankAngle - bankAngle_) * kEnemyBankResponse;
    rotation_ = ToEulerRotation(forward_, bankAngle_);

    attackTimer_++;
    if (!isRetreating && isInAttackRange && attackTimer_ >= kAttackInterval && bulletManager) {
        Vector3 shootDirection = NormalizeOr(Add(Scale(directionToPlayer, 0.9f), Scale(forward_, 0.1f)), directionToPlayer);
        Vector3 bulletPosition = Add(position_, Scale(forward_, 1.2f));
        Vector3 bulletVelocity = Scale(shootDirection, kEnemyBulletSpeed);
        bulletManager->Shoot(bulletPosition, bulletVelocity);
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


