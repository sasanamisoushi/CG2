#include "Enemy.h"
#include "3D/Object3dCommon.h"

void Enemy::Initialize(const Vector3 &position) {
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dCommon::GetInstance());

    // 自機と同じBoxモデルを使い回す
    object_->SetModel("EnemyBox");

    // 敵っぽく赤色にする
    if (object_->GetModel()) {
        object_->GetModel()->SetColor({ 0.8f, 0.1f, 0.1f, 1.0f });
    }

    // 的として当てやすいように自機より少し大きくする
    object_->SetScale({ 2.0f, 2.0f, 2.0f });

    position_ = position;
}

void Enemy::Update() {
    // 今は動かないので、座標をセットして更新するだけ
    object_->SetTranslate(position_);
    object_->Update();
}

void Enemy::Draw() {
    if (object_) {
        object_->Draw();
    }
}