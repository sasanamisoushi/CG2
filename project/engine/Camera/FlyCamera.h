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

    // マウス操作パラメーター
    void SetMouseSensitivity(float sens) { mouseSensitivity_ = sens; }
    void SetScrollSpeed(float speed) { scrollSpeed_ = speed; }
    void SetPanSpeed(float speed) { panSpeed_ = speed; }

    float GetMoveSpeed() const { return moveSpeed_; }
    float GetRotateSpeed() const { return rotateSpeed_; }
    float GetMouseSensitivity() const { return mouseSensitivity_; }
    float GetScrollSpeed() const { return scrollSpeed_; }
    float GetPanSpeed() const { return panSpeed_; }

    static void SetGameViewHovered(bool isHovered) { sGameViewHovered_ = isHovered; }
    static void SetGameViewBounds(float minX, float minY, float maxX, float maxY);
    static void SubmitGameViewMouseInput(
        bool isHovered,
        bool isRightClicked,
        bool isMiddleClicked,
        bool isRightDown,
        bool isMiddleDown,
        float deltaX,
        float deltaY,
        float wheel);

private:
    float moveSpeed_ = 0.5f;
    float rotateSpeed_ = 0.03f;

    // マウス操作パラメーター
    float mouseSensitivity_ = 0.003f;  // 右ドラッグ回転感度
    float scrollSpeed_ = 1.5f;         // ホイール前後移動速度
    float panSpeed_ = 0.05f;           // 中クリックパン速度

    // カメラ自身の現在のクォータニオン
    Quaternion quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };

    static bool sGameViewHovered_;
    static bool sGameViewMouseCaptured_;
    static bool sGameViewBoundsValid_;
    static float sGameViewMinX_;
    static float sGameViewMinY_;
    static float sGameViewMaxX_;
    static float sGameViewMaxY_;
    static float sPendingMouseDeltaX_;
    static float sPendingMouseDeltaY_;
    static float sPendingMouseWheel_;
    static bool sPendingRightDown_;
    static bool sPendingMiddleDown_;
};

