#include "Obstacle.h"
#include "3D/Object3dCommon.h"
#include "3D/ModelManager.h"

void Obstacle::Initialize(const std::string& modelName, const Vector3& position, const Vector3& rotation, const Vector3& scale) {
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dCommon::GetInstance());
    
    // 指定されたモデルを割り当てる（無ければデフォルトの "ObstacleBox" を使う）
    if (ModelManager::GetInstance()->FindModel(modelName) != nullptr) {
        object_->SetModel(modelName);
    } else {
        object_->SetModel("ObstacleBox");
    }

    position_ = position;
    rotation_ = rotation;
    scale_ = scale;

    object_->SetTranslate(position_);
    object_->SetRotate(rotation_);
    object_->SetScale(scale_);
}

void Obstacle::Update() {
    if (object_) {
        object_->SetTranslate(position_);
        object_->SetRotate(rotation_);
        object_->SetScale(scale_);
        object_->Update();
    }
}

void Obstacle::Draw() {
    if (object_) {
        object_->Draw();
    }
}
