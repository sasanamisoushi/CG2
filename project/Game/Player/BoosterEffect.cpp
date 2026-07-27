#include "BoosterEffect.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include <algorithm>
#include <cmath>

void BoosterEffect::Initialize() {
    lastMode_ = -1;
    time_ = 0.0f;
}

void BoosterEffect::SetupBurnersForMode(int playerMode) {
    burners_.clear();
    lastMode_ = playerMode;

    const float scale = 0.08f; // vf-15cのスケール

    if (playerMode == 0) { // Fighter
        // 後ろノズル左右2箇所
        Burner b1, b2;
        
        b1.trail = std::make_unique<Trail>();
        b1.trail->Initialize(15);
        b1.trailObject = std::make_unique<Object3d>();
        b1.trailObject->Initialize(Object3dCommon::GetInstance());
        b1.trailObject->SetModel("SmokeTrail");
        b1.offset = { -1.2f * scale, 0.0f * scale, -4.5f * scale };
        b1.defaultScale = { 0.25f * scale, 0.25f * scale, 1.5f * scale };
        b1.currentScale = b1.defaultScale;
        b1.color = { 0.2f, 0.5f, 1.0f, 0.9f }; // ファイターは青白い炎
        burners_.push_back(std::move(b1));

        b2.trail = std::make_unique<Trail>();
        b2.trail->Initialize(15);
        b2.trailObject = std::make_unique<Object3d>();
        b2.trailObject->Initialize(Object3dCommon::GetInstance());
        b2.trailObject->SetModel("SmokeTrail");
        b2.offset = { 1.2f * scale, 0.0f * scale, -4.5f * scale };
        b2.defaultScale = { 0.25f * scale, 0.25f * scale, 1.5f * scale };
        b2.currentScale = b2.defaultScale;
        b2.color = { 0.2f, 0.5f, 1.0f, 0.9f };
        burners_.push_back(std::move(b2));

    } else if (playerMode == 1) { // Gerwalk
        Burner b1, b2;
        
        b1.trail = std::make_unique<Trail>();
        b1.trail->Initialize(12);
        b1.trailObject = std::make_unique<Object3d>();
        b1.trailObject->Initialize(Object3dCommon::GetInstance());
        b1.trailObject->SetModel("SmokeTrail");
        b1.offset = { -1.5f * scale, -3.5f * scale, -1.5f * scale };
        b1.defaultScale = { 0.22f * scale, 0.22f * scale, 1.2f * scale };
        b1.currentScale = b1.defaultScale;
        b1.color = { 1.0f, 0.5f, 0.1f, 0.8f }; // ホバリングはオレンジ
        burners_.push_back(std::move(b1));

        b2.trail = std::make_unique<Trail>();
        b2.trail->Initialize(12);
        b2.trailObject = std::make_unique<Object3d>();
        b2.trailObject->Initialize(Object3dCommon::GetInstance());
        b2.trailObject->SetModel("SmokeTrail");
        b2.offset = { 1.5f * scale, -3.5f * scale, -1.5f * scale };
        b2.defaultScale = { 0.22f * scale, 0.22f * scale, 1.2f * scale };
        b2.currentScale = b2.defaultScale;
        b2.color = { 1.0f, 0.5f, 0.1f, 0.8f };
        burners_.push_back(std::move(b2));

    } else { // Battroid
        Burner b1, b2;
        
        b1.trail = std::make_unique<Trail>();
        b1.trail->Initialize(10);
        b1.trailObject = std::make_unique<Object3d>();
        b1.trailObject->Initialize(Object3dCommon::GetInstance());
        b1.trailObject->SetModel("SmokeTrail");
        b1.offset = { -0.8f * scale, 1.5f * scale, -2.0f * scale };
        b1.defaultScale = { 0.18f * scale, 0.18f * scale, 1.0f * scale };
        b1.currentScale = b1.defaultScale;
        b1.color = { 1.0f, 0.3f, 0.1f, 0.85f }; // バトロイドは赤みが強い炎
        burners_.push_back(std::move(b1));

        b2.trail = std::make_unique<Trail>();
        b2.trail->Initialize(10);
        b2.trailObject = std::make_unique<Object3d>();
        b2.trailObject->Initialize(Object3dCommon::GetInstance());
        b2.trailObject->SetModel("SmokeTrail");
        b2.offset = { 0.8f * scale, 1.5f * scale, -2.0f * scale };
        b2.defaultScale = { 0.18f * scale, 0.18f * scale, 1.0f * scale };
        b2.currentScale = b2.defaultScale;
        b2.color = { 1.0f, 0.3f, 0.1f, 0.85f };
        burners_.push_back(std::move(b2));
    }
}

void BoosterEffect::Update(const Vector3& position, const Quaternion& rotation, int playerMode, float speedRatio, bool isAccelerating) {
    if (playerMode != lastMode_) {
        SetupBurnersForMode(playerMode);
    }

    time_ += 0.4f;

    float cappedSpeedRatio = speedRatio > 1.2f ? 1.2f : speedRatio;
    float targetScaleZ = cappedSpeedRatio * 1.5f + (isAccelerating ? 2.5f : 0.2f);
    
    float noise = std::sin(time_) * 0.08f + (float)(rand() % 8) * 0.01f;
    targetScaleZ += noise;
    if (targetScaleZ < 0.1f) targetScaleZ = 0.1f;

    float targetAlpha = cappedSpeedRatio * 0.6f + (isAccelerating ? 0.8f : 0.15f);
    targetAlpha = std::clamp(targetAlpha + noise * 0.2f, 0.0f, 1.0f);

    Matrix4x4 rotationMatrix = MyMath::MakeRotateMatrix(rotation);

    for (auto& burner : burners_) {
        burner.currentScale.z += (burner.defaultScale.z * targetScaleZ - burner.currentScale.z) * 0.3f;
        float targetXY = burner.defaultScale.x * (isAccelerating ? 1.2f : 0.8f);
        burner.currentScale.x += (targetXY - burner.currentScale.x) * 0.3f;
        burner.currentScale.y += (targetXY - burner.currentScale.y) * 0.3f;

        Vector3 worldOffset = MyMath::Transform(burner.offset, rotationMatrix);
        Vector3 worldPosition = { position.x + worldOffset.x, position.y + worldOffset.y, position.z + worldOffset.z };

        if (burner.trail) {
            burner.trail->Update(worldPosition);
            // 色と太さを更新に保存しておく（Drawで使うため）
            burner.color.w = targetAlpha;
        }
    }
}

void BoosterEffect::Draw(Camera* camera) {
    if (!camera) return;

    for (auto& burner : burners_) {
        if (burner.trail && burner.trailObject && burner.trailObject->GetModel()) {
            float width = burner.currentScale.x * 20.0f;
            if (width < 0.01f) width = 0.01f;
            std::vector<VertexData> trailVertices = burner.trail->GenerateVertices(camera, width);
            
            // 色の適用（Trailの頂点に色が反映されるかはモデルの実装次第だが、ここではモデルの色を設定しておく）
            burner.trailObject->GetModel()->SetColor(burner.color);
            
            burner.trailObject->GetModel()->UpdateTrailVertices(trailVertices);
            burner.trailObject->Update();
            
            burner.trailObject->Draw();
        }
    }
}
