#include "EnemyActionController.h"
#include "Enemy.h"

namespace {
constexpr float kEnemyCruiseSpeed = 0.05f;

Vector3 Scale(const Vector3& value, float scalar) {
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}
}

void EnemyActionController::Destroy(Enemy& enemy) const {
    enemy.isDead_ = true;
}

void EnemyActionController::TakeDamage(Enemy& enemy, int damage) const {
    if (enemy.isDead_ || damage <= 0) return;
    StartChasing(enemy);
    enemy.hp_ -= damage;
    if (enemy.hp_ <= 0) {
        enemy.hp_ = 0;
        Destroy(enemy);
    }
}

void EnemyActionController::StartChasing(Enemy& enemy) const {
    if (enemy.isDead_) return;
    enemy.isChasingPlayer_ = true;
    enemy.state_ = EnemyState::Approach;
    enemy.currentSpeed_ = (enemy.currentSpeed_ > 0.0f) ? enemy.currentSpeed_ : kEnemyCruiseSpeed;
    enemy.velocity_ = Scale(enemy.forward_, enemy.currentSpeed_);
}
