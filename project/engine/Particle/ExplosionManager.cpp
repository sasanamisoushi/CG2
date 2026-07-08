#include "ExplosionManager.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace {
constexpr bool kEnableExplosionMeshes = true;
}

void ExplosionManager::Initialize(ParticleManager *particleManager) {
    particleManager_ = particleManager;

    // アプリ起動時に前回の設定があれば読み込む
    LoadFromJson("resources/explosionConfig.json");
    if (particleManager_) {
        particleManager_->CreateParticleGroup("explosionCore", "resources/circle2.png");
    }

    if (kEnableExplosionMeshes) {
        // ヒット演出用のモデルは全Explosionで共有する。
        ModelManager::GetInstance()->CreateRingModel(
            "Explosion_OrbitRing", 64, 1.0f, 0.72f, true,
            { 1.0f, 0.4f, 0.05f, 0.0f }, { 1.0f, 1.0f, 0.75f, 0.95f });
        ModelManager::GetInstance()->CreateCylinderModel(
            "Explosion_Cylinder", 32, 0.9f, 0.75f, 1.5f);

        if (Model *cylinderModel = ModelManager::GetInstance()->FindModel("Explosion_Cylinder")) {
            cylinderModel->SetColor({ 1.0f, 0.25f, 0.02f, 0.25f });
            cylinderModel->SetAlphaReference(0.0f);
        }

        for (Explosion &explosion : explosions_) {
            InitializeExplosionObjects(explosion);
            explosion.active = false;
            explosion.age = 0.0f;
            explosion.duration = 0.7f;
        }
    }
}

void ExplosionManager::InitializeExplosionObjects(Explosion &explosion) {
    Object3dCommon *object3dCommon = Object3dCommon::GetInstance();
    for (auto &ring : explosion.rings) {
        if (!ring) {
            ring = std::make_unique<Object3d>();
            ring->Initialize(object3dCommon);
        }
        ring->SetModel("Explosion_OrbitRing");
        ring->SetScale({ 0.0f, 0.0f, 0.0f });
        ring->Update();
    }

    if (!explosion.cylinder) {
        explosion.cylinder = std::make_unique<Object3d>();
        explosion.cylinder->Initialize(object3dCommon);
    }
    explosion.cylinder->SetModel("Explosion_Cylinder");
    explosion.cylinder->SetScale({ 0.0f, 0.0f, 0.0f });
    explosion.cylinder->Update();
}

ExplosionManager::Explosion *ExplosionManager::AcquireExplosion() {
    Explosion *reuseTarget = nullptr;

    for (Explosion &explosion : explosions_) {
        if (!explosion.active) {
            reuseTarget = &explosion;
            break;
        }

        if (!reuseTarget || explosion.age > reuseTarget->age) {
            reuseTarget = &explosion;
        }
    }

    if (!reuseTarget) {
        return nullptr;
    }

    InitializeExplosionObjects(*reuseTarget);
    reuseTarget->active = true;
    reuseTarget->age = 0.0f;
    reuseTarget->duration = 0.7f;
    return reuseTarget;
}

void ExplosionManager::CreateExplosions(const std::vector<Vector3> &hitPositions) {
    CreateDestructionEffects(hitPositions);
}

void ExplosionManager::CreateHitEffects(const std::vector<Vector3> &hitPositions) {
    CreateEffects(hitPositions, false);
}

void ExplosionManager::CreateDestructionEffects(const std::vector<Vector3> &hitPositions) {
    CreateEffects(hitPositions, true);
}

void ExplosionManager::CreateEffects(const std::vector<Vector3> &hitPositions, bool includeMeshes) {
    if (!particleManager_) return;

    for (const Vector3 &pos : hitPositions) {
        particleManager_->Emit("test", pos, config_.count, 
            Vector4{ config_.color[0], config_.color[1], config_.color[2], config_.color[3] },
            config_.speed, config_.speedVariance,
            config_.scale, config_.scaleVariance,
            config_.lifeTimeMin, config_.lifeTimeMax,
            config_.posVariance);
        particleManager_->EmitFireball(
            "explosionCore", pos, 14, { 1.0f, 0.32f, 0.04f, 0.72f },
            0.45f, 2.6f, 0.7f, 0.22f);

        if (includeMeshes && kEnableExplosionMeshes) {
            Explosion *explosion = AcquireExplosion();
            if (!explosion) {
                continue;
            }

            explosion->position = pos;
            for (auto &ring : explosion->rings) {
                if (!ring) {
                    continue;
                }
                ring->SetModel("Explosion_OrbitRing");
                ring->SetTranslate(pos);
                ring->SetScale({ 0.05f, 0.05f, 0.05f });
            }

            if (explosion->cylinder) {
                explosion->cylinder->SetModel("Explosion_Cylinder");
                explosion->cylinder->SetTranslate(pos);
                explosion->cylinder->SetScale({ 0.2f, 0.2f, 0.2f });
            }
        }
    }
}

void ExplosionManager::Update() {
    if (!kEnableExplosionMeshes) {
        return;
    }

    constexpr float kDeltaTime = 1.0f / 60.0f;

    for (Explosion &explosion : explosions_) {
        if (!explosion.active) {
            continue;
        }

        explosion.age += kDeltaTime;
        if (explosion.age >= explosion.duration) {
            explosion.active = false;
            for (auto &ring : explosion.rings) {
                if (ring) {
                    ring->SetScale({ 0.0f, 0.0f, 0.0f });
                    ring->Update();
                }
            }
            if (explosion.cylinder) {
                explosion.cylinder->SetScale({ 0.0f, 0.0f, 0.0f });
                explosion.cylinder->Update();
            }
            continue;
        }

        const float progress = explosion.age / explosion.duration;
        const float pulse = std::sin(progress * 3.14159265f);
        const float ringScale = pulse * 3.0f;
        const float cylinderRadius = pulse * 1.8f;
        const float cylinderHeight = pulse * 0.28f;

        if (explosion.rings[0]) {
            explosion.rings[0]->SetScale({ ringScale, ringScale, ringScale });
            explosion.rings[0]->SetRotate({ 0.75f, 0.15f, progress * 2.5f });
            explosion.rings[0]->Update();
        }

        if (explosion.rings[1]) {
            explosion.rings[1]->SetScale({ ringScale * 0.92f, ringScale * 0.92f, ringScale * 0.92f });
            explosion.rings[1]->SetRotate({ -0.5f, 0.4f, -progress * 2.1f });
            explosion.rings[1]->Update();
        }

        if (explosion.rings[2]) {
            explosion.rings[2]->SetScale({ ringScale * 0.82f, ringScale * 0.82f, ringScale * 0.82f });
            explosion.rings[2]->SetRotate({ 0.2f, -0.65f, progress * 1.8f });
            explosion.rings[2]->Update();
        }

        if (explosion.cylinder) {
            explosion.cylinder->SetTranslate({
                explosion.position.x,
                explosion.position.y - cylinderHeight * 0.75f,
                explosion.position.z
            });
            explosion.cylinder->SetScale({ cylinderRadius, cylinderHeight, cylinderRadius });
            explosion.cylinder->SetRotate({ 0.0f, progress * 1.5f, 0.0f });
            explosion.cylinder->Update();
        }
    }
}

void ExplosionManager::Draw() {
    if (!kEnableExplosionMeshes) {
        return;
    }

    for (const Explosion &explosion : explosions_) {
        if (!explosion.active) {
            continue;
        }

        for (const auto &ring : explosion.rings) {
            if (ring) {
                ring->Draw();
            }
        }
        if (explosion.cylinder) {
            explosion.cylinder->Draw();
        }
    }
}

void ExplosionManager::SaveToJson(const std::string &filepath) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return;

    ofs << "{\n";
    ofs << "  \"count\": " << config_.count << ",\n";
    ofs << "  \"color\": [" 
        << config_.color[0] << ", " 
        << config_.color[1] << ", " 
        << config_.color[2] << ", " 
        << config_.color[3] << "],\n";
    ofs << "  \"speed\": " << config_.speed << ",\n";
    ofs << "  \"speedVariance\": " << config_.speedVariance << ",\n";
    ofs << "  \"scale\": " << config_.scale << ",\n";
    ofs << "  \"scaleVariance\": " << config_.scaleVariance << ",\n";
    ofs << "  \"lifeTimeMin\": " << config_.lifeTimeMin << ",\n";
    ofs << "  \"lifeTimeMax\": " << config_.lifeTimeMax << ",\n";
    ofs << "  \"posVariance\": " << config_.posVariance << "\n";
    ofs << "}\n";
}

void ExplosionManager::LoadFromJson(const std::string &filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;

    std::string line;
    while (std::getline(ifs, line)) {
        // キーと値の区切りを検索
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        // キーを取得
        std::string key = line.substr(0, colonPos);
        // クォーテーションを除去
        size_t firstQuote = key.find('"');
        size_t lastQuote = key.rfind('"');
        if (firstQuote == std::string::npos || lastQuote == std::string::npos || firstQuote == lastQuote) continue;
        key = key.substr(firstQuote + 1, lastQuote - firstQuote - 1);

        // 値の部分を取得
        std::string valStr = line.substr(colonPos + 1);
        // 末尾のカンマやスペース、閉じブラケット等を除去
        while (!valStr.empty() && (valStr.back() == ',' || valStr.back() == ' ' || valStr.back() == '\r' || valStr.back() == '\n' || valStr.back() == '}')) {
            valStr.pop_back();
        }
        // 先頭のスペースを除去
        size_t firstNonSpace = valStr.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) {
            valStr = valStr.substr(firstNonSpace);
        }

        if (key == "count") {
            config_.count = std::stoi(valStr);
        } else if (key == "color") {
            // [r, g, b, a] のパース
            size_t openBracket = valStr.find('[');
            size_t closeBracket = valStr.find(']');
            if (openBracket != std::string::npos && closeBracket != std::string::npos) {
                std::string arrayContent = valStr.substr(openBracket + 1, closeBracket - openBracket - 1);
                std::stringstream ss(arrayContent);
                std::string item;
                int i = 0;
                while (std::getline(ss, item, ',') && i < 4) {
                    config_.color[i++] = std::stof(item);
                }
            }
        } else if (key == "speed") {
            config_.speed = std::stof(valStr);
        } else if (key == "speedVariance") {
            config_.speedVariance = std::stof(valStr);
        } else if (key == "scale") {
            config_.scale = std::stof(valStr);
        } else if (key == "scaleVariance") {
            config_.scaleVariance = std::stof(valStr);
        } else if (key == "lifeTimeMin") {
            config_.lifeTimeMin = std::stof(valStr);
        } else if (key == "lifeTimeMax") {
            config_.lifeTimeMax = std::stof(valStr);
        } else if (key == "posVariance") {
            config_.posVariance = std::stof(valStr);
        }
    }
}
