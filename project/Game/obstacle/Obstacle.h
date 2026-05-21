#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include <memory>
#include <string>

class Obstacle {
public:
    // 初期化
    void Initialize(const std::string& modelName, const Vector3& position, const Vector3& rotation = {0.0f, 0.0f, 0.0f}, const Vector3& scale = {1.0f, 1.0f, 1.0f});

    // 更新
    void Update();

    // 描画
    void Draw();

    // ゲッター
    Vector3 GetPosition() const { return position_; }
    Vector3 GetRotation() const { return rotation_; }
    Vector3 GetScale() const { return scale_; }

    // モデルのローカルバウンディングボックス半径 × scale = ワールド当たり判定の AABB extents
    Vector3 GetWorldHalfExtents() const {
        auto absf = [](float value) { return value < 0.0f ? -value : value; };
        Vector3 absScale = { absf(scale_.x), absf(scale_.y), absf(scale_.z) };

        if (object_ && object_->GetModel()) {
            Vector3 localHalf = object_->GetModel()->GetHalfExtents();
            return { localHalf.x * absScale.x, localHalf.y * absScale.y, localHalf.z * absScale.z };
        }
        return absScale;
        // フォールバック: スケールそのまま（元サイズ2と仮定した半径）
        return scale_;
    }

    // セッター
    void SetPosition(const Vector3& position) { position_ = position; }
    void SetRotation(const Vector3& rotation) { rotation_ = rotation; }
    void SetScale(const Vector3& scale) { scale_ = scale; }

private:
    std::unique_ptr<Object3d> object_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
};
