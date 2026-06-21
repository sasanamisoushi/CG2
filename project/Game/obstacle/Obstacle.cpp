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
    UpdateMeshCollider();
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
    obb.orientations[0] = { 1.0f, 0.0f, 0.0f };
    obb.orientations[1] = { 0.0f, 1.0f, 0.0f };
    obb.orientations[2] = { 0.0f, 0.0f, 1.0f };
    obb.size = GetWorldHalfExtents();

    if (object_ && object_->GetModel()) {
        Vector3 localCenter = object_->GetModel()->GetBoundsCenter();
        Vector3 scaledCenter = { 
            (localCenter.x + collisionOffset_.x) * scale_.x, 
            (localCenter.y + collisionOffset_.y) * scale_.y, 
            (localCenter.z + collisionOffset_.z) * scale_.z 
        };
        Vector3 rotatedCenter = MyMath::Transform(scaledCenter, MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::Multiply(MyMath::MakeRotateYMatrix(rotation_.y), MyMath::MakeRotateZMatrix(rotation_.z))));
        obb.center = { position_.x + rotatedCenter.x, position_.y + rotatedCenter.y, position_.z + rotatedCenter.z };

        Matrix4x4 rotMat = MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::Multiply(MyMath::MakeRotateYMatrix(rotation_.y), MyMath::MakeRotateZMatrix(rotation_.z)));
        obb.orientations[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
        obb.orientations[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
        obb.orientations[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
    }
    return obb;
}

void Obstacle::UpdateMeshCollider() {
    if (!useMeshCollider_) return;
    
    if (prevPosition_.x == position_.x && prevPosition_.y == position_.y && prevPosition_.z == position_.z &&
        prevRotation_.x == rotation_.x && prevRotation_.y == rotation_.y && prevRotation_.z == rotation_.z &&
        prevScale_.x == scale_.x && prevScale_.y == scale_.y && prevScale_.z == scale_.z && 
        !worldTriangles_.empty()) {
        return;
    }

    worldTriangles_.clear();
    if (!object_ || !object_->GetModel()) return;
    
    const auto& modelData = object_->GetModel()->GetModelData();
    if (modelData.vertices.empty()) return;

    Matrix4x4 rotMat = MyMath::Multiply(MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::MakeRotateYMatrix(rotation_.y)), MyMath::MakeRotateZMatrix(rotation_.z));
    Matrix4x4 worldMatrix = MyMath::Multiply(MyMath::Multiply(MyMath::MkeScaleMatrix(scale_), rotMat), MyMath::MakeTranslateMatrix(position_));

    auto getTransformedPos = [&](const Vector4& localPos) -> Vector3 {
        Vector3 v = { localPos.x, localPos.y, localPos.z };
        return MyMath::Transform(v, worldMatrix);
    };

    if (!modelData.indices.empty()) {
        for (size_t i = 0; i < modelData.indices.size(); i += 3) {
            Triangle t;
            t.p[0] = getTransformedPos(modelData.vertices[modelData.indices[i]].position);
            t.p[1] = getTransformedPos(modelData.vertices[modelData.indices[i+1]].position);
            t.p[2] = getTransformedPos(modelData.vertices[modelData.indices[i+2]].position);
            Vector3 v1 = MyMath::Subtract(t.p[1], t.p[0]);
            Vector3 v2 = MyMath::Subtract(t.p[2], t.p[0]);
            t.normal = MyMath::Normalize(MyMath::Cross(v1, v2));
            worldTriangles_.push_back(t);
        }
    } else {
        for (size_t i = 0; i < modelData.vertices.size(); i += 3) {
            if (i + 2 >= modelData.vertices.size()) break;
            Triangle t;
            t.p[0] = getTransformedPos(modelData.vertices[i].position);
            t.p[1] = getTransformedPos(modelData.vertices[i+1].position);
            t.p[2] = getTransformedPos(modelData.vertices[i+2].position);
            Vector3 v1 = MyMath::Subtract(t.p[1], t.p[0]);
            Vector3 v2 = MyMath::Subtract(t.p[2], t.p[0]);
            t.normal = MyMath::Normalize(MyMath::Cross(v1, v2));
            worldTriangles_.push_back(t);
        }
    }
    
    prevPosition_ = position_;
    prevRotation_ = rotation_;
    prevScale_ = scale_;
}
