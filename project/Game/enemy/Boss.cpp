#include "Boss.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/EnemyBulletManager.h"
#include <cmath>

namespace {
Vector3 AddVector(const Vector3 &a, const Vector3 &b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 ScaleVector(const Vector3 &v, float s) { return { v.x * s, v.y * s, v.z * s }; }

// 元の戦闘用スケール (0.40) を基準に、プレイヤーの約4倍まで拡大する。
// X/Z を少し大きくして、横に広く厚みのある艦体にする。
constexpr Vector3 kBossScale = { 2.00f, 1.60f, 2.20f };
constexpr float kBossSidewaysYaw = 1.57079632679f; // 90 degrees
}

void Boss::Initialize(const Vector3 &position) {
    Enemy::Initialize(position);

    // 通常敵の箱モデルではなく、ボス専用の巨大戦艦モデルを使う。
    constexpr char kBossModel[] = "FortressBattleshipBoss.obj";
    ModelManager *modelManager = ModelManager::GetInstance();
    if (!modelManager->FindModel(kBossModel)) {
        modelManager->LoadModel(kBossModel);
    }
    object_->SetModel(kBossModel);

    SetIsBoss(true);
    hp_ = kMaxHP;
    SetScale(kBossScale);
    // 艦首をプレイヤーの進行方向に対して横へ向ける。
    SetRotation({ 0.0f, kBossSidewaysYaw, 0.0f });
    actionTimer_ = 0;
    summonRequests_ = 0;
    UpdateModel();
}

void Boss::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    if (isDead_) return;

    ++actionTimer_;
    position_.y += std::sin(static_cast<float>(actionTimer_) * 0.012f) * 0.008f;
    const int cycle = actionTimer_ % 720;
    if (cycle == 90 || cycle == 150) FireCannon(playerPos, bulletManager);
    if (cycle == 300) summonRequests_ += 3;
    if (cycle == 570) FireBeam(playerPos, bulletManager);

    CheckCollision(obstacles);
    UpdateModel();
}

Vector3 Boss::DirectionTo(const Vector3 &target) const {
    Vector3 d{ target.x - position_.x, target.y - position_.y, target.z - position_.z };
    const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    return length > 0.0001f ? ScaleVector(d, 1.0f / length) : Vector3{ 0.0f, 0.0f, -1.0f };
}

void Boss::FireCannon(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {
    if (!bulletManager) return;
    const Vector3 direction = DirectionTo(playerPos);
    bulletManager->ShootHeavyCannon(AddVector(position_, ScaleVector(direction, 14.0f)), ScaleVector(direction, 0.32f));
}

void Boss::FireBeam(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {
    if (!bulletManager) return;
    const Vector3 direction = DirectionTo(playerPos);
    bulletManager->ShootBeam(AddVector(position_, ScaleVector(direction, 18.0f)), ScaleVector(direction, 0.22f));
}

int Boss::ConsumeSummonRequests() {
    const int result = summonRequests_;
    summonRequests_ = 0;
    return result;
}

void Boss::UpdateModel() {
    Enemy::UpdateModel();
}

void Boss::Draw() {
    Enemy::Draw();
}
