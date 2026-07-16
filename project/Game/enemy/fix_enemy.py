with open('Enemy.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

del lines[436:690]

fix = '''        attackTimer_ = 0;
    }
}

void Enemy::UpdateFlightPathAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {
    if (flightPathPoints_.size() < 2) {
        hasFlightPath_ = false;
        return;
    }

    const size_t segmentCount = flightPathLoop_ ? flightPathPoints_.size() : flightPathPoints_.size() - 1;
    if (segmentCount == 0) {
        return;
    }

    Vector3 oldPosition = position_;
    Vector3 oldForward = forward_;

    float remainingDistance = flightPathSpeed_;
    while (remainingDistance > 0.0f) {
        size_t nextIndex = ResolvePathIndex(static_cast<int>(flightPathSegmentIndex_ + 1), flightPathPoints_.size(), flightPathLoop_);
        Vector3 segmentVector = Subtract(flightPathPoints_[nextIndex], flightPathPoints_[flightPathSegmentIndex_]);
        float segmentLength = Length(segmentVector);
        
        if (segmentLength < 0.0001f) {
            ++flightPathSegmentIndex_;
            flightPathSegmentT_ = 0.0f;
            if (flightPathSegmentIndex_ >= segmentCount) {
                if (flightPathLoop_) {
                    flightPathSegmentIndex_ = 0;
                } else {
                    flightPathSegmentIndex_ = segmentCount - 1;
                    flightPathSegmentT_ = 1.0f;
                    break;
                }
            }
            continue;
        }

        float distanceToSegmentEnd = segmentLength * (1.0f - flightPathSegmentT_);
        if (remainingDistance >= distanceToSegmentEnd) {
            remainingDistance -= distanceToSegmentEnd;
            flightPathSegmentT_ = 0.0f;
            ++flightPathSegmentIndex_;
            if (flightPathSegmentIndex_ >= segmentCount) {
                if (flightPathLoop_) {
                    flightPathSegmentIndex_ = 0;
                } else {
                    flightPathSegmentIndex_ = segmentCount - 1;
                    flightPathSegmentT_ = 1.0f;
                    break;
                }
            }
        } else {
            flightPathSegmentT_ += remainingDistance / segmentLength;
            remainingDistance = 0.0f;
        }
    }

    position_ = EvaluateFlightPath(flightPathPoints_, flightPathSegmentIndex_, flightPathSegmentT_, flightPathLoop_);
    Vector3 pathForward = NormalizeOr(Subtract(position_, oldPosition), oldForward);
    forward_ = pathForward;
    currentSpeed_ = flightPathSpeed_;
    velocity_ = Scale(forward_, currentSpeed_);

    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
    float yawTurn = MyMath::Dot(MyMath::Cross(oldForward, forward_), worldUp);
    float targetBankAngle = std::clamp(-yawTurn * 18.0f, -kEnemyMaxBankAngle, kEnemyMaxBankAngle);
    bankAngle_ += (targetBankAngle - bankAngle_) * kEnemyBankResponse;
    rotation_ = ToEulerRotation(forward_, bankAngle_);

    Vector3 toPlayer = Subtract(playerPos, position_);
    float distSqToPlayer = LengthSq(toPlayer);
    Vector3 directionToPlayer = NormalizeOr(toPlayer, forward_);
    bool isInAttackRange = distSqToPlayer <= kAttackRadius * kAttackRadius;
    state_ = isInAttackRange ? EnemyState::Attack : EnemyState::Approach;

    attackTimer_++;
    if (isInAttackRange && attackTimer_ >= kAttackInterval && bulletManager) {
        ShootAtTarget(bulletManager, position_, playerPos, directionToPlayer);
        attackTimer_ = 0;
    }
}

void Enemy::CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    // =========================================================
    //    敵と障害物の当たり判定
    // =========================================================
    OBB enemyOBB = GetOBB();
'''

lines.insert(436, fix)
with open('Enemy.cpp', 'w', encoding='utf-8') as f:
    f.writelines(lines)

