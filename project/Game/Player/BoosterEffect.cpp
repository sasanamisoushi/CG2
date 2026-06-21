#include "BoosterEffect.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include <cmath>
#include <algorithm>

void BoosterEffect::Initialize() {
    // 炎を表現する円錐モデルを生成 (先端を極めて細くしたCylinder)
    // 名前: "BoosterFlame"
    ModelManager* mm = ModelManager::GetInstance();
    if (mm->FindModel("BoosterFlame") == nullptr) {
        // topRadius=0.01f, bottomRadius=1.0f, height=2.0f の円錐
        mm->CreateCylinderModel("BoosterFlame", 16, 0.01f, 1.0f, 2.0f);
    }
}

void BoosterEffect::SetupBurnersForMode(int playerMode) {
    burners_.clear();
    lastMode_ = playerMode;

    const float scale = 0.08f; // vf-15cのスケール

    if (playerMode == 0) { // Fighter
        // 後ろノズル左右2箇所
        // ネイティブ座標で X=±1.2f, Y=0.0f, Z=-4.5f
        Burner b1, b2;
        b1.object = std::make_unique<Object3d>();
        b1.object->Initialize(Object3dCommon::GetInstance());
        b1.object->SetModel("BoosterFlame");
        b1.offset = { -1.2f * scale, 0.0f * scale, -4.5f * scale };
        b1.defaultScale = { 0.25f * scale, 0.25f * scale, 1.5f * scale };
        b1.currentScale = b1.defaultScale;
        b1.color = { 0.1f, 0.7f, 1.0f, 0.9f }; // ファイターは青白い炎
        burners_.push_back(std::move(b1));

        b2.object = std::make_unique<Object3d>();
        b2.object->Initialize(Object3dCommon::GetInstance());
        b2.object->SetModel("BoosterFlame");
        b2.offset = { 1.2f * scale, 0.0f * scale, -4.5f * scale };
        b2.defaultScale = { 0.25f * scale, 0.25f * scale, 1.5f * scale };
        b2.currentScale = b2.defaultScale;
        b2.color = { 0.1f, 0.7f, 1.0f, 0.9f };
        burners_.push_back(std::move(b2));

    } else if (playerMode == 1) { // Gerwalk
        // 脚部（足の裏）後方ノズル左右2箇所
        // ネイティブ座標で X=±1.5f, Y=-3.5f, Z=-1.5f
        Burner b1, b2;
        b1.object = std::make_unique<Object3d>();
        b1.object->Initialize(Object3dCommon::GetInstance());
        b1.object->SetModel("BoosterFlame");
        b1.offset = { -1.5f * scale, -3.5f * scale, -1.5f * scale };
        b1.defaultScale = { 0.22f * scale, 0.22f * scale, 1.2f * scale };
        b1.currentScale = b1.defaultScale;
        b1.color = { 1.0f, 0.5f, 0.1f, 0.8f }; // ホバリングはオレンジ
        burners_.push_back(std::move(b1));

        b2.object = std::make_unique<Object3d>();
        b2.object->Initialize(Object3dCommon::GetInstance());
        b2.object->SetModel("BoosterFlame");
        b2.offset = { 1.5f * scale, -3.5f * scale, -1.5f * scale };
        b2.defaultScale = { 0.22f * scale, 0.22f * scale, 1.2f * scale };
        b2.currentScale = b2.defaultScale;
        b2.color = { 1.0f, 0.5f, 0.1f, 0.8f };
        burners_.push_back(std::move(b2));

    } else { // Battroid
        // 背面のメインバックパックバーニア左右2箇所
        // ネイティブ座標で X=±0.8f, Y=1.5f, Z=-2.0f
        Burner b1, b2;
        b1.object = std::make_unique<Object3d>();
        b1.object->Initialize(Object3dCommon::GetInstance());
        b1.object->SetModel("BoosterFlame");
        b1.offset = { -0.8f * scale, 1.5f * scale, -2.0f * scale };
        b1.defaultScale = { 0.18f * scale, 0.18f * scale, 1.0f * scale };
        b1.currentScale = b1.defaultScale;
        b1.color = { 1.0f, 0.3f, 0.1f, 0.85f }; // バトロイドは赤みが強い炎
        burners_.push_back(std::move(b1));

        b2.object = std::make_unique<Object3d>();
        b2.object->Initialize(Object3dCommon::GetInstance());
        b2.object->SetModel("BoosterFlame");
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

    time_ += 0.4f; // アニメーション明滅用

    // 速度や入力に応じたスケール変更
    float targetScaleZ = speedRatio * 1.5f + (isAccelerating ? 0.8f : 0.2f);
    // ゆらぎを追加
    float noise = std::sin(time_) * 0.08f + (float)(rand() % 8) * 0.01f;
    targetScaleZ += noise;
    if (targetScaleZ < 0.1f) targetScaleZ = 0.1f;

    // アルファ値も速度に連動
    float targetAlpha = speedRatio * 0.6f + (isAccelerating ? 0.4f : 0.15f);
    targetAlpha = std::clamp(targetAlpha + noise * 0.2f, 0.0f, 1.0f);

    Matrix4x4 rotationMatrix = MyMath::MakeRotateMatrix(rotation);

    for (auto& burner : burners_) {
        // 現在スケールをターゲットに補間
        burner.currentScale.z += (burner.defaultScale.z * targetScaleZ - burner.currentScale.z) * 0.3f;
        burner.currentScale.x += (burner.defaultScale.x * (speedRatio * 0.5f + 0.8f) - burner.currentScale.x) * 0.3f;
        burner.currentScale.y += (burner.defaultScale.y * (speedRatio * 0.5f + 0.8f) - burner.currentScale.y) * 0.3f;

        // ワールド座標へのオフセット変換
        Vector3 worldOffset = MyMath::Transform(burner.offset, rotationMatrix);
        Vector3 worldPosition = { position.x + worldOffset.x, position.y + worldOffset.y, position.z + worldOffset.z };

        // 円柱モデルはY軸方向（高さ）に生成される。
        // ノズル後方に噴射させるため、Y軸が機体後方（-Z）を向くよう、ローカルX軸に90度(PI/2)回転を加える。
        Quaternion q90X = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, 3.14159265f * 0.5f);
        Quaternion finalQ = MyMath::Multiply(q90X, rotation);

        if (burner.object) {
            burner.object->SetTranslate(worldPosition);
            burner.object->SetScale(burner.currentScale);
            burner.object->SetQuaternionRotate(finalQ);

            // アルファ値を設定して色をモデルに適用
            Vector4 finalColor = burner.color;
            finalColor.w = targetAlpha;
            if (burner.object->GetModel()) {
                burner.object->GetModel()->SetColor(finalColor);
            }

            burner.object->Update();
        }
    }
}

void BoosterEffect::Draw() {
    for (auto& burner : burners_) {
        if (burner.object) {
            burner.object->Draw();
        }
    }
}
