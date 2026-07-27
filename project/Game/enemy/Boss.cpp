#include "Boss.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/EnemyBulletManager.h"
#include <cmath>

namespace {
Vector3 AddVector(const Vector3 &a, const Vector3 &b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 ScaleVector(const Vector3 &v, float s) { return { v.x * s, v.y * s, v.z * s }; }
}

void Boss::Initialize(const Vector3 &position) {
    Enemy::Initialize(position);
    SetIsBoss(true);
    hp_ = 60;
    SetScale({ 4.0f, 2.0f, 12.0f });
    actionTimer_ = 0;
    summonRequests_ = 0;

    for (auto &part : hullParts_) {
        part = std::make_unique<Object3d>();
        part->Initialize(Object3dCommon::GetInstance());
        part->SetModel("BossHull");
    }
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
    const std::array<Vector3, 6> offsets = {{
        { -6.0f, 0.0f, 1.0f }, { 6.0f, 0.0f, 1.0f },
        { -9.0f, -0.5f, -2.0f }, { 9.0f, -0.5f, -2.0f },
        { 0.0f, 3.0f, -2.0f }, { 0.0f, -2.0f, 7.0f }
    }};
    const std::array<Vector3, 6> scales = {{
        { 5.0f, 0.8f, 7.0f }, { 5.0f, 0.8f, 7.0f },
        { 2.0f, 1.2f, 4.0f }, { 2.0f, 1.2f, 4.0f },
        { 2.2f, 2.0f, 3.0f }, { 2.5f, 1.5f, 5.0f }
    }};
    for (size_t i = 0; i < hullParts_.size(); ++i) {
        hullParts_[i]->SetTranslate(AddVector(position_, offsets[i]));
        hullParts_[i]->SetRotate(rotation_);
        hullParts_[i]->SetScale(scales[i]);
        hullParts_[i]->Update();
    }
}

void Boss::Draw() {
    Enemy::Draw();
    for (const auto &part : hullParts_) {
        if (part) part->Draw();
    }
}
