#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include <memory>
#include <list>


class Obstacle;


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
    void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles);

    // 描画
    void Draw();

    // ミサイルに狙われるためのゲッター
    Vector3 GetPosition() const { return position_; }
    Vector3 GetRotation() const { return rotation_; }
    Vector3 GetScale() const { return scale_; }

    Vector3 GetWorldHalfExtents() const {
        auto absf = [](float value) { return value < 0.0f ? -value : value; };
        Vector3 absScale = { absf(scale_.x), absf(scale_.y), absf(scale_.z) };

        if (object_ && object_->GetModel()) {
            Vector3 localHalf = object_->GetModel()->GetHalfExtents();
            return { localHalf.x * absScale.x, localHalf.y * absScale.y, localHalf.z * absScale.z };
        }

        return absScale;
    }

    // ミサイルと衝突したときの処理
    void OnCollision();

    // 死んだかどうかのゲッター
    bool IsDead() const { return isDead_; }

    // サイズ（スケール）を設定するセッター
    void SetScale(const Vector3 &scale) { scale_ = scale; if (object_) object_->SetScale(scale_); }

    // 回転
    void SetRotation(const Vector3 &rotation) { rotation_ = rotation; }

    // 思考・行動処理
    void UpdateAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager); 
   
    // 当たり判定処理
    void CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles); 

private:

    // AI用の変数
    EnemyState state_ = EnemyState::Approach; // 今の状態（最初は「接近」）
    int attackTimer_ = 0;                     // 攻撃の間隔を測るタイマー

    std::unique_ptr<Object3d> object_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };

    // 回転を保持する変数
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

    // 死んだかどうかのフラグ
    bool isDead_ = false;
};

