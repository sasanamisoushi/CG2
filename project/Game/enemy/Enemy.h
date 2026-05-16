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

    // ★重要：ミサイルに狙われるためのゲッター
    Vector3 GetPosition() const { return position_; }

private:
    std::unique_ptr<Object3d> object_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
};

