#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include <memory>


class EnemyBulletManager;

// 敵の行動パターン（状態）を定義
enum class EnemyState {
    Approach, // プレイヤーに近づく
    Attack    // 攻撃する
};

class Enemy {
public:
    // 初期化（発生位置を渡す）
    void Initialize(const Vector3 &position);

    // 毎フレームの更新
    void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager);

    // 描画
    void Draw();

    // ミサイルに狙われるためのゲッター
    Vector3 GetPosition() const { return position_; }

    // ミサイルと衝突したときの処理
    void OnCollision();

    // 死んだかどうかのゲッター
    bool IsDead() const { return isDead_; }

    // サイズ（スケール）を設定するセッター
    void SetScale(const Vector3 &scale) { if (object_) object_->SetScale(scale); }

private:

    // ★追加：AI用の変数
    EnemyState state_ = EnemyState::Approach; // 今の状態（最初は「接近」）
    int attackTimer_ = 0;                     // 攻撃の間隔を測るタイマー

    std::unique_ptr<Object3d> object_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };

   

    // 死んだかどうかのフラグ
    bool isDead_ = false;
};

