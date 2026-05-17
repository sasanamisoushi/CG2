#include "Player.h"
#include "3D/Object3dCommon.h"

void Player::Initialize(const std::string &modelName) {
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dCommon::GetInstance());
    object_->SetModel(modelName);

    if (object_->GetModel()) {
        object_->GetModel()->SetColor({ 0.2f, 0.5f, 1.0f, 1.0f }); // 青色
    }


    object_->SetScale({ 0.2f, 0.2f, 0.2f });

    position_ = { 0.0f, 0.0f, 0.0f };
    quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };
}

void Player::Update() {
    auto input = Input::GetInstance();

    // ==========================================
    // 1. 回転の処理 (矢印キーと Q/E)
    // ==========================================
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;

    if (input->PushKey(DIK_UP))    pitch -= rotSpeed_; // 機首下げ
    if (input->PushKey(DIK_DOWN))  pitch += rotSpeed_; // 機首上げ
    if (input->PushKey(DIK_RIGHT)) yaw += rotSpeed_; // 右旋回
    if (input->PushKey(DIK_LEFT))  yaw -= rotSpeed_; // 左旋回
    if (input->PushKey(DIK_E))     roll -= rotSpeed_; // 右ロール
    if (input->PushKey(DIK_Q))     roll += rotSpeed_; // 左ロール

  
    // 各軸ごとの回転クォータニオンを作成して合成
    Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
    Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
    Quaternion qRoll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, roll);

    quaternion_ = MyMath::Multiply(quaternion_, qPitch);
    quaternion_ = MyMath::Multiply(quaternion_, qYaw);
    quaternion_ = MyMath::Multiply(quaternion_, qRoll);
    quaternion_ = MyMath::Normalize(quaternion_);

    // ==========================================
    // 2. 移動の処理 (Wキーで前進、Sキーでブレーキ/後退)
    // ==========================================
    Vector3 moveInput = { 0.0f, 0.0f, 0.0f };

    // ① どのキーが押されているかチェック
    if (input->PushKey(DIK_W)) moveInput.z += 1.0f;
    if (input->PushKey(DIK_S)) moveInput.z -= 1.0f;
    if (input->PushKey(DIK_D)) moveInput.x += 1.0f;
    if (input->PushKey(DIK_A)) moveInput.x -= 1.0f;

    // ② 斜め移動の加速を防ぐ（長さを1にする）
    if (moveInput.x != 0.0f || moveInput.z != 0.0f) {
        moveInput = MyMath::Normalize(moveInput);
    }

    // ③ 入力方向を、自機の向いている方向に合わせて曲げる
    Vector3 velocity = MyMath::RotateVector(moveInput, quaternion_);

    // ④ スピードを掛けて足し込む
    position_.x += velocity.x * speed_;
    position_.y += velocity.y * speed_;
    position_.z += velocity.z * speed_;

    // 3. Object3dに座標と回転を適用
    object_->SetTranslate(position_);
    object_->SetQuaternionRotate(quaternion_);
    object_->Update();
}

void Player::Draw() {
    if (object_) {
        object_->Draw();
    }
}

void Player::UpdateCamera(Camera *camera) {
    // 機体から見たカメラの相対位置（真後ろに10m、上に3m）
    Vector3 offset = { 0.0f, 0.0f, -10.0f };

    // このオフセットを、機体の傾きに合わせて回転させる
    Vector3 rotatedOffset = MyMath::RotateVector(offset, quaternion_);

    // カメラの最終的な座標 ＝ プレイヤーの座標 ＋ 回転させたオフセット
    Vector3 camPos = {
        position_.x + rotatedOffset.x,
        position_.y + rotatedOffset.y,
        position_.z + rotatedOffset.z
    };

    camera->SetTranslate(camPos);

    // カメラの向きも機体と全く同じにする
    camera->SetRotate({ 0,0,0 }); // オイラー角のリセット(ハイブリッド対応のため)
    camera->SetQuaternion(quaternion_); // ※Camera.hに実装した関数
}

Vector3 Player::GetForwardVector() const {
    return MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);
}
