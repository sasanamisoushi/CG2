#pragma once
#include "3D/Object3d.h"
#include "engine/Input/Input.h"
#include "engine/Camera/Camera.h"
#include "engine/math/MyMath.h"
#include <memory>
#include <string>

class Player {
public:
    // 初期化（読み込むモデルの名前を渡す）
    void Initialize(const std::string &modelName);

    // 毎フレームの更新（キーボード入力による移動と回転）
    void Update();

    // 描画
    void Draw();

    // カメラを自機の背後に追従させる
    void UpdateCamera(Camera *camera);

    // ゲッター
    Vector3 GetPosition() const { return position_; }
    Quaternion GetQuaternion() const { return quaternion_; }
    Vector3 GetForwardVector() const; // 今向いている方向（ミサイル発射などに使う）

    void OnCollision();
    bool IsDead() const { return isDead_; }

private:
    std::unique_ptr<Object3d> object_;

    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Quaternion quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f }; // 単位クォータニオン（無回転）

    float speed_ = 0.2f;       // 前進スピード
    float rotSpeed_ = 0.05f;   // 旋回スピード

    bool isDead_ = false;
};

