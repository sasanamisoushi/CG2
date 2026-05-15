#include "FlyCamera.h"
#include "engine/Input/Input.h"

FlyCamera::FlyCamera() {
	// 初期状態は原点、正面向き
    SetTranslate({ 0.0f, 0.0f, 0.0f });
}

void FlyCamera::Update() {
    auto input = Input::GetInstance();

    // ==========================================
    // 1. 回転の計算 (矢印キーとQ/Eキー)
    // ==========================================
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;

    if (input->PushKey(DIK_UP))    pitch -= rotateSpeed_; // 上を向く
    if (input->PushKey(DIK_DOWN))  pitch += rotateSpeed_; // 下を向く
    if (input->PushKey(DIK_RIGHT)) yaw += rotateSpeed_; // 右旋回
    if (input->PushKey(DIK_LEFT))  yaw -= rotateSpeed_; // 左旋回
    if (input->PushKey(DIK_Q))     roll -= rotateSpeed_; // 左ロール
    if (input->PushKey(DIK_E))     roll += rotateSpeed_; // 右ロール

    // カメラの現在のローカル軸を取得
    Vector3 localRight = MyMath::RotateVector({ 1.0f, 0.0f, 0.0f }, quaternion_);
    Vector3 localUp = MyMath::RotateVector({ 0.0f, 1.0f, 0.0f }, quaternion_);
    Vector3 localForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);

    // 軸に基づいて回転を合成
    Quaternion qPitch = MyMath::MakeAxisAngle(localRight, pitch);
    Quaternion qYaw = MyMath::MakeAxisAngle(localUp, yaw);
    Quaternion qRoll = MyMath::MakeAxisAngle(localForward, roll);

    quaternion_ = MyMath::Multiply(quaternion_, qPitch);
    quaternion_ = MyMath::Multiply(quaternion_, qYaw);
    quaternion_ = MyMath::Multiply(quaternion_, qRoll);


    // ==========================================
    // 2. 移動の計算 (WASDキー)
    // ==========================================
    Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
    if (input->PushKey(DIK_W)) moveDir.z += 1.0f; // 前
    if (input->PushKey(DIK_S)) moveDir.z -= 1.0f; // 後ろ
    if (input->PushKey(DIK_D)) moveDir.x += 1.0f; // 右
    if (input->PushKey(DIK_A)) moveDir.x -= 1.0f; // 左

    // 入力があった場合、現在のカメラの向きに合わせて移動させる
    if (moveDir.x != 0.0f || moveDir.z != 0.0f) {
        moveDir = MyMath::Normalize(moveDir);

        // カメラの回転を考慮した移動ベクトルに変換
        Vector3 rotatedMove = MyMath::RotateVector(moveDir, quaternion_);

        // 現在の座標を取得して加算し、セットし直す
        Vector3 currentPos = GetTranslate();
        currentPos.x += rotatedMove.x * moveSpeed_;
        currentPos.y += rotatedMove.y * moveSpeed_;
        currentPos.z += rotatedMove.z * moveSpeed_;
        SetTranslate(currentPos);
    }

    // ==========================================
    // 3. 親クラスのUpdateを呼んで行列を更新
    // ==========================================
    this->SetQuaternion(quaternion_); // ハイブリッド機能を利用
    Camera::Update();
}
