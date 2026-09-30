#include "BoosterEffect.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kPlayerModelScale = 0.08f;
}

void BoosterEffect::Initialize() {
    lastMode_ = -1;
    time_ = 0.0f;

    // 常時表示する炎は、トレイルとは別の軽量なプリミティブで作る。
    // 1つのモデルを左右ノズルの Object3d から共有しているため、描画コストを抑えられる。
    ModelManager* modelManager = ModelManager::GetInstance();
    modelManager->CreateSphereModel("PlayerThrusterCore", 12);
    if (Model* core = modelManager->FindModel("PlayerThrusterCore")) {
        core->SetTextureFilePath("resources/white1x1.png");
        core->SetEnableLighting(false);
        core->SetAlphaReference(0.0f);
        core->SetColor({ 0.92f, 0.98f, 1.0f, 1.0f });
    }

    // Y方向に細くなる円すい。各ノズルでは回転して、機体後方へ伸ばす。
    modelManager->CreateCylinderModel("PlayerThrusterFlame", 16, 0.035f, 0.16f, 1.0f);
    if (Model* flame = modelManager->FindModel("PlayerThrusterFlame")) {
        flame->SetTextureFilePath("resources/white1x1.png");
        flame->SetEnableLighting(false);
        flame->SetAlphaReference(0.0f);
        flame->SetColor({ 0.08f, 0.55f, 1.0f, 0.92f });
    }
}

void BoosterEffect::SetupBurnersForMode(int playerMode) {
    burners_.clear();
    lastMode_ = playerMode;

    const float scale = kPlayerModelScale;

    const auto makeBurner = [](const Vector3& offset, const Vector3& defaultScale,
        const Vector4& color, int trailLength) {
        Burner burner;
        burner.trail = std::make_unique<Trail>();
        burner.trail->Initialize(trailLength);
        burner.trailObject = std::make_unique<Object3d>();
        burner.trailObject->Initialize(Object3dCommon::GetInstance());
        burner.trailObject->SetModel("SmokeTrail");

        burner.coreObject = std::make_unique<Object3d>();
        burner.coreObject->Initialize(Object3dCommon::GetInstance());
        burner.coreObject->SetModel("PlayerThrusterCore");
        burner.flameObject = std::make_unique<Object3d>();
        burner.flameObject->Initialize(Object3dCommon::GetInstance());
        burner.flameObject->SetModel("PlayerThrusterFlame");

        burner.offset = offset;
        // ゲーム内のプレイヤー前方はローカル +Z。三人称カメラはその反対側
        // （-Z、機体後方）に置かれるため、噴射も -Z 側へ伸ばす。
        burner.exhaustDirection = { 0.0f, 0.0f, -1.0f };
        burner.defaultScale = defaultScale;
        burner.currentScale = defaultScale;
        burner.color = color;
        return burner;
    };

    if (playerMode == 0) { // Fighter: 胴体後部の左右ノズル
        burners_.push_back(makeBurner({ -1.2f * scale, 0.0f, -4.5f * scale }, { 0.25f * scale, 0.25f * scale, 1.5f * scale }, { 0.2f, 0.5f, 1.0f, 0.9f }, 15));
        burners_.push_back(makeBurner({  1.2f * scale, 0.0f, -4.5f * scale }, { 0.25f * scale, 0.25f * scale, 1.5f * scale }, { 0.2f, 0.5f, 1.0f, 0.9f }, 15));
    } else if (playerMode == 1) { // Gerwalk: 脚部後方の左右ノズル
        burners_.push_back(makeBurner({ -1.5f * scale, -3.5f * scale, -1.5f * scale }, { 0.22f * scale, 0.22f * scale, 1.2f * scale }, { 0.2f, 0.5f, 1.0f, 0.9f }, 12));
        burners_.push_back(makeBurner({  1.5f * scale, -3.5f * scale, -1.5f * scale }, { 0.22f * scale, 0.22f * scale, 1.2f * scale }, { 0.2f, 0.5f, 1.0f, 0.9f }, 12));
    } else { // Battroid: 背部の左右ノズル
        burners_.push_back(makeBurner({ -0.8f * scale, 1.5f * scale, -2.0f * scale }, { 0.18f * scale, 0.18f * scale, 1.0f * scale }, { 0.2f, 0.5f, 1.0f, 0.9f }, 10));
        burners_.push_back(makeBurner({  0.8f * scale, 1.5f * scale, -2.0f * scale }, { 0.18f * scale, 0.18f * scale, 1.0f * scale }, { 0.2f, 0.5f, 1.0f, 0.9f }, 10));
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
        const Vector3 worldExhaust = MyMath::RotateVector(burner.exhaustDirection, rotation);

        // 移動していなくても見える白いコアと青い外炎。
        const float flameLength = kPlayerModelScale * (5.2f + cappedSpeedRatio * 5.0f + (isAccelerating ? 7.0f : 0.0f) + noise * 3.0f);
        const float flameRadius = kPlayerModelScale * (0.90f + cappedSpeedRatio * 0.28f + (isAccelerating ? 0.36f : 0.0f));
        if (burner.coreObject) {
            burner.coreObject->SetTranslate(worldPosition);
            const float coreRadius = kPlayerModelScale * (0.64f + cappedSpeedRatio * 0.14f + (isAccelerating ? 0.20f : 0.0f));
            burner.coreObject->SetScale({ coreRadius, coreRadius, coreRadius });
            burner.coreObject->Update();
        }
        if (burner.flameObject) {
            // 円すいのローカル +Y 軸をローカル -Z（機体後方）へ回してから、機体姿勢を重ねる。
            const Quaternion localFlameAxis = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, -1.57079632679f);
            // 根元がノズルに埋まらないよう、後方へわずかに押し出す。
            burner.flameObject->SetTranslate({
                worldPosition.x + worldExhaust.x * kPlayerModelScale * 0.05f,
                worldPosition.y + worldExhaust.y * kPlayerModelScale * 0.05f,
                worldPosition.z + worldExhaust.z * kPlayerModelScale * 0.05f
            });
            burner.flameObject->SetScale({ flameRadius, flameLength, flameRadius });
            burner.flameObject->SetQuaternionRotate(MyMath::Multiply(rotation, localFlameAxis));
            burner.flameObject->Update();
        }

        if (burner.trail) {
            burner.trail->Update(worldPosition);
            // 色と太さを更新に保存しておく（Drawで使うため）
            burner.color.w = targetAlpha;
        }
    }
}

void BoosterEffect::Draw(Camera* camera) {
    if (!camera) return;

    // ノズル直後の光は、機体の陰に入っても完全には消えないよう深度なしで描く。
    // 移動方向を示すトレイルだけは奥行きを保つため通常のエフェクト描画へ戻す。
    Object3dCommon* object3dCommon = Object3dCommon::GetInstance();
    object3dCommon->SetOverlayEffectDrawSettings();
    for (auto& burner : burners_) {
        if (burner.flameObject) {
            burner.flameObject->Draw();
        }
        if (burner.coreObject) {
            burner.coreObject->Draw();
        }
    }

    object3dCommon->SetEffectDrawSettings();
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
