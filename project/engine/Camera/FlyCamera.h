#pragma once
#include "engine/Camera/Camera.h"

class FlyCamera : public Camera {
public:
    FlyCamera();

    // 毎フレームの更新（入力処理を含む）
    void Update() override;

    // 移動・回転速度の調整
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    void SetRotateSpeed(float speed) { rotateSpeed_ = speed; }

private:
    float moveSpeed_ = 0.5f;
    float rotateSpeed_ = 0.03f;

    // カメラ自身の現在のクォータニオン
    Quaternion quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };
};

