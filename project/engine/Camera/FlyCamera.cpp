#include "FlyCamera.h"
#include "engine/Input/Input.h"
#include <Windows.h>
#include <externals/imgui/imgui.h>

bool FlyCamera::sGameViewHovered_ = false;
bool FlyCamera::sGameViewMouseCaptured_ = false;
bool FlyCamera::sGameViewBoundsValid_ = false;
float FlyCamera::sGameViewMinX_ = 0.0f;
float FlyCamera::sGameViewMinY_ = 0.0f;
float FlyCamera::sGameViewMaxX_ = 0.0f;
float FlyCamera::sGameViewMaxY_ = 0.0f;
float FlyCamera::sPendingMouseDeltaX_ = 0.0f;
float FlyCamera::sPendingMouseDeltaY_ = 0.0f;
float FlyCamera::sPendingMouseWheel_ = 0.0f;
bool FlyCamera::sPendingRightDown_ = false;
bool FlyCamera::sPendingMiddleDown_ = false;

FlyCamera::FlyCamera() {
	// 蛻晄悄迥ｶ諷九・蜴溽せ縲∵ｭ｣髱｢蜷代″
    SetTranslate({ 0.0f, 0.0f, 0.0f });
}

void FlyCamera::SetGameViewBounds(float minX, float minY, float maxX, float maxY) {
    sGameViewMinX_ = minX;
    sGameViewMinY_ = minY;
    sGameViewMaxX_ = maxX;
    sGameViewMaxY_ = maxY;
    sGameViewBoundsValid_ = true;
}

bool FlyCamera::GetGameViewBounds(float &minX, float &minY, float &maxX, float &maxY) {
    if (!sGameViewBoundsValid_) return false;
    minX = sGameViewMinX_;
    minY = sGameViewMinY_;
    maxX = sGameViewMaxX_;
    maxY = sGameViewMaxY_;
    return true;
}

bool FlyCamera::GetGameViewMousePos(float mouseX, float mouseY, Vector2& outPos) {
    if (!sGameViewBoundsValid_) return false;
    if (mouseX < sGameViewMinX_ || mouseX > sGameViewMaxX_ || mouseY < sGameViewMinY_ || mouseY > sGameViewMaxY_) return false;
    outPos.x = mouseX - sGameViewMinX_;
    outPos.y = mouseY - sGameViewMinY_;
    return true;
}

void FlyCamera::GetGameViewSize(float& width, float& height) {
    width = sGameViewMaxX_ - sGameViewMinX_;
    height = sGameViewMaxY_ - sGameViewMinY_;
}

void FlyCamera::SubmitGameViewMouseInput(
    bool isHovered,
    bool isRightClicked,
    bool isMiddleClicked,
    bool isRightDown,
    bool isMiddleDown,
    float deltaX,
    float deltaY,
    float wheel) {
    if (isHovered && (isRightClicked || isMiddleClicked)) {
        sGameViewMouseCaptured_ = true;
    }
    if (!isRightDown && !isMiddleDown) {
        sGameViewMouseCaptured_ = false;
    }

    bool canUseMouse = isHovered || sGameViewMouseCaptured_;
    sPendingRightDown_ = canUseMouse && isRightDown;
    sPendingMiddleDown_ = canUseMouse && isMiddleDown;
    sPendingMouseDeltaX_ = canUseMouse ? deltaX : 0.0f;
    sPendingMouseDeltaY_ = canUseMouse ? deltaY : 0.0f;
    sPendingMouseWheel_ = canUseMouse ? wheel : 0.0f;
}

void FlyCamera::Update() {
    auto* input = Input::GetInstance();

    // ==========================================
    // 1. 蝗櫁ｻ｢縺ｮ險育ｮ・
    //    繧ｭ繝ｼ繝懊・繝・(遏｢蜊ｰ繧ｭ繝ｼ/Q/E) + 繝槭え繧ｹ蜿ｳ繝峨Λ繝・げ
    // ==========================================
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;

    // --- 繧ｭ繝ｼ繝懊・繝牙屓霆｢ ---
    if (canUseKeyboard_) {
        if (input->PushKey(DIK_UP))    pitch -= rotateSpeed_;
        if (input->PushKey(DIK_DOWN))  pitch += rotateSpeed_;
        if (input->PushKey(DIK_RIGHT)) yaw   += rotateSpeed_;
        if (input->PushKey(DIK_LEFT))  yaw   -= rotateSpeed_;
        if (input->PushKey(DIK_Q))     roll  -= rotateSpeed_;

        if (input->PushKey(DIK_E))     roll  += rotateSpeed_;
    }

    bool isShiftDown = input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT);
    if (sPendingRightDown_ || (sPendingMiddleDown_ && !isShiftDown)) {
        yaw += sPendingMouseDeltaX_ * mouseSensitivity_;
        pitch += sPendingMouseDeltaY_ * mouseSensitivity_;
    }

    Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
    Quaternion qYaw   = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
    Quaternion qRoll  = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, roll);

        quaternion_ = MyMath::Multiply(quaternion_, qPitch);
    quaternion_ = MyMath::Multiply(quaternion_, qYaw);
    quaternion_ = MyMath::Multiply(quaternion_, qRoll);
    quaternion_ = MyMath::Normalize(quaternion_);

    // ==========================================
    // 2. 遘ｻ蜍輔・險育ｮ・
    //    WASD + 繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ繝帙う繝ｼ繝ｫ (蜑榊ｾ・ + 荳ｭ繝峨Λ繝・げ (繝代Φ)
    // ==========================================
    // 繧ｫ繝｡繝ｩ縺ｮ蜷代″繧定ｨ育ｮ暦ｼ亥屓霆｢蠕鯉ｼ・
    Vector3 localRight   = MyMath::RotateVector({ 1.0f, 0.0f, 0.0f }, quaternion_);
    Vector3 localUp      = MyMath::RotateVector({ 0.0f, 1.0f, 0.0f }, quaternion_);
    Vector3 localForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);

    Vector3 currentPos = GetTranslate();

    // --- WASD 遘ｻ蜍・---
    Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
    if (canUseKeyboard_) {
        if (input->PushKey(DIK_W)) moveDir.z += 1.0f;
        if (input->PushKey(DIK_S)) moveDir.z -= 1.0f;
        if (input->PushKey(DIK_D)) moveDir.x += 1.0f;
        if (input->PushKey(DIK_A)) moveDir.x -= 1.0f;
    }

    if (moveDir.x != 0.0f || moveDir.z != 0.0f) {
        moveDir = MyMath::Normalize(moveDir);
        Vector3 rotatedMove = MyMath::RotateVector(moveDir, quaternion_);
        currentPos.x += rotatedMove.x * moveSpeed_;
        currentPos.y += rotatedMove.y * moveSpeed_;
        currentPos.z += rotatedMove.z * moveSpeed_;
    }

    // ==========================================
    // --- 繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ繝帙う繝ｼ繝ｫ: 繧ｫ繝｡繝ｩ蜑榊ｾ檎ｧｻ蜍・---
    // ==========================================
    // ImGui縺ｮ繝帙う繝ｼ繝ｫ讖溯・繧剃ｽｿ縺・
    float wheel = sPendingMouseWheel_;
    if (wheel != 0.0f) {
        float scrollStep = wheel * scrollSpeed_;
        currentPos.x += localForward.x * scrollStep;
        currentPos.y += localForward.y * scrollStep;
        currentPos.z += localForward.z * scrollStep;
    }

    // ==========================================
     // --- 荳ｭ繧ｯ繝ｪ繝・け繝峨Λ繝・げ: 繝代Φ・井ｸ贋ｸ句ｷｦ蜿ｳ蟷ｳ陦檎ｧｻ蜍包ｼ・--
     // ==========================================
     //ImGui縺ｮ繝峨Λ繝・げ讖溯・繧剃ｽｿ縺・
    if (sPendingMiddleDown_ && isShiftDown) {
        float deltaX = sPendingMouseDeltaX_;
        float deltaY = sPendingMouseDeltaY_;

        float panStep = panSpeed_;
        currentPos.x -= localRight.x * deltaX * panStep;
        currentPos.y -= localRight.y * deltaX * panStep;
        currentPos.z -= localRight.z * deltaX * panStep;

        currentPos.x += localUp.x * deltaY * panStep;
        currentPos.y += localUp.y * deltaY * panStep;
        currentPos.z += localUp.z * deltaY * panStep;
    }

    SetTranslate(currentPos);
    sPendingMouseDeltaX_ = 0.0f;
    sPendingMouseDeltaY_ = 0.0f;
    sPendingMouseWheel_ = 0.0f;

    // ==========================================
    // 3. 隕ｪ繧ｯ繝ｩ繧ｹ縺ｮUpdate繧貞他繧薙〒陦悟・繧呈峩譁ｰ
    // ==========================================
    this->SetQuaternion(quaternion_);
    Camera::Update();
}

void FlyCamera::AddYawPitch(float yaw, float pitch) {
    Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
    Quaternion qYaw   = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
    quaternion_ = MyMath::Multiply(quaternion_, qPitch);
    quaternion_ = MyMath::Multiply(quaternion_, qYaw);
    quaternion_ = MyMath::Normalize(quaternion_);
    this->SetQuaternion(quaternion_);
}
