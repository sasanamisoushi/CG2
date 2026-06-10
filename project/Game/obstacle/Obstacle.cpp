#include "Obstacle.h"
#include "3D/Object3dCommon.h"
#include "3D/ModelManager.h"
#include <vector>

void Obstacle::Initialize(const std::string& modelName, const Vector3& position, const Vector3& rotation, const Vector3& scale) {
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dCommon::GetInstance());

    if (modelName.find("StageBounds") != std::string::npos) {
        isStageBounds_ = true;

        const std::string wireModelName = "StageBoundsWire";
        ModelManager::GetInstance()->CreateLineModel(wireModelName);

        if (Model* wireModel = ModelManager::GetInstance()->FindModel(wireModelName)) {
            std::vector<VertexData> vertices;
            vertices.reserve(24);

            Vector4 color = { 0.0f, 1.0f, 1.0f, 1.0f };
            Vector3 corners[] = {
                { -1.0f, -1.0f, -1.0f }, {  1.0f, -1.0f, -1.0f },
                {  1.0f,  1.0f, -1.0f }, { -1.0f,  1.0f, -1.0f },
                { -1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f,  1.0f },
                {  1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f,  1.0f },
            };

            auto addLine = [&](int start, int end) {
                VertexData v1{};
                VertexData v2{};
                v1.position = { corners[start].x, corners[start].y, corners[start].z, 1.0f };
                v2.position = { corners[end].x, corners[end].y, corners[end].z, 1.0f };
                v1.color = color;
                v2.color = color;
                vertices.push_back(v1);
                vertices.push_back(v2);
            };

            addLine(0, 1); addLine(1, 2); addLine(2, 3); addLine(3, 0);
            addLine(4, 5); addLine(5, 6); addLine(6, 7); addLine(7, 4);
            addLine(0, 4); addLine(1, 5); addLine(2, 6); addLine(3, 7);
            wireModel->UpdateLineVertices(vertices);
        }

        object_->SetModel(wireModelName);
    } else if (ModelManager::GetInstance()->FindModel(modelName) != nullptr) {
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
    object_->Update();
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
    if (!object_) {
        return;
    }

    object_->Draw();
}

OBB Obstacle::GetOBB() const {
    OBB obb;
    obb.center = position_;
    obb.size = GetWorldHalfExtents();
    Matrix4x4 rotMat = MyMath::Multiply(MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::MakeRotateYMatrix(rotation_.y)), MyMath::MakeRotateZMatrix(rotation_.z));
    obb.orientations[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    obb.orientations[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
    obb.orientations[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
    return obb;
}
