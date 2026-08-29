#pragma once
#include "Game/enemy/Enemy.h"
#include "3D/Model.h"
#include "3D/Animation.h"
#include <memory>

class GroundEnemy : public Enemy {
public:
    GroundEnemy() = default;
    ~GroundEnemy() override = default;

    void Initialize(const Vector3 &position) override;
    void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) override;
    void Draw() override;
    void UpdateModel() override;
    void UpdateGroundMovement(const Vector3 &playerPos, const std::list<std::unique_ptr<Obstacle>> &obstacles);
    void SnapToGround(const std::list<std::unique_ptr<Obstacle>> &obstacles);
    void UpdateFlightPathMovement();

    // 近接攻撃判定ブロックのOBBを取得（当たりのチェック用）
    OBB GetMeleeBoxOBB() const;
    bool IsMeleeActive() const { return isMeleeActive_; }

private:
    void UpdateAttackAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager);

    // 重力とジャンプ
    float velocityY_ = 0.0f;
    float gravity_ = 0.08f;
    bool isGrounded_ = false;
    float groundY_ = 0.0f;
    int jumpTimer_ = 0;
    int jumpInterval_ = 180; // 約3秒に1回ハイジャンプ機会

    // 攻撃関連
    enum class GroundAttackState {
        Idle,
        Missile,
        Gatling,
        Melee
    };
    GroundAttackState attackSubState_ = GroundAttackState::Idle;
    int attackTimer_ = 0;
    int gatlingShotCount_ = 0;
    int gatlingIntervalTimer_ = 0;

    // 近接攻撃用当たり判定ブロック
    std::unique_ptr<Object3d> meleeBox_;
    Vector3 meleeBoxPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 meleeBoxScale_ = { 0.22f, 0.22f, 0.32f };
    bool isMeleeActive_ = false;
    int meleeTimer_ = 0;

    // プレイヤーの変形から独立した独自スキニング姿勢（VF3バトロイド固定）
    Animation animationData_{};
    Skeleton skeleton_{};
    SkinCluster skinCluster_{};
    bool enableSkinning_ = false;
};
