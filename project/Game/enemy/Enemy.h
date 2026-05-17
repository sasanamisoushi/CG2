#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include <memory>

class Enemy {
public:
    // 初期化（発生位置を渡す）
    void Initialize(const Vector3 &position);

    // 毎フレームの更新
    void Update();

    // 描画
    void Draw();

    // ミサイルに狙われるためのゲッター
    Vector3 GetPosition() const { return position_; }

    // ミサイルと衝突したときの処理
    void OnCollision();

    // 死んだかどうかのゲッター
    bool IsDead() const { return isDead_; }

private:
    std::unique_ptr<Object3d> object_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };

    // 死んだかどうかのフラグ
    bool isDead_ = false;
};

