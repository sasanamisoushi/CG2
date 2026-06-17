#include "ExplosionManager.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>

void ExplosionManager::Initialize(ParticleManager *particleManager) {
    particleManager_ = particleManager;
    explosions_.clear();

    // アプリ起動時に前回の設定があれば読み込む
    LoadFromJson("resources/explosionConfig.json");
    particleManager_->CreateParticleGroup("explosionCore", "resources/circle2.png");

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
}

void ExplosionManager::CreateExplosions(const std::vector<Vector3> &hitPositions) {
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

        Explosion explosion;
        explosion.position = pos;
        for (auto &ring : explosion.rings) {
            ring = std::make_unique<Object3d>();
            ring->Initialize(Object3dCommon::GetInstance());
            ring->SetModel("Explosion_OrbitRing");
            ring->SetTranslate(pos);
            ring->SetScale({ 0.05f, 0.05f, 0.05f });
        }

        explosion.cylinder = std::make_unique<Object3d>();
        explosion.cylinder->Initialize(Object3dCommon::GetInstance());
        explosion.cylinder->SetModel("Explosion_Cylinder");
        explosion.cylinder->SetTranslate(pos);
        explosion.cylinder->SetScale({ 0.2f, 0.2f, 0.2f });

        explosions_.push_back(std::move(explosion));
    }
}

void ExplosionManager::Update() {
    constexpr float kDeltaTime = 1.0f / 60.0f;

    for (auto it = explosions_.begin(); it != explosions_.end();) {
        it->age += kDeltaTime;
        if (it->age >= it->duration) {
            it = explosions_.erase(it);
            continue;
        }

        const float progress = it->age / it->duration;
        const float pulse = std::sin(progress * 3.14159265f);
        const float ringScale = pulse * 3.0f;
        const float cylinderRadius = pulse * 1.8f;
        const float cylinderHeight = pulse * 0.28f;

        it->rings[0]->SetScale({ ringScale, ringScale, ringScale });
        it->rings[0]->SetRotate({ 0.75f, 0.15f, progress * 2.5f });
        it->rings[0]->Update();

        it->rings[1]->SetScale({ ringScale * 0.92f, ringScale * 0.92f, ringScale * 0.92f });
        it->rings[1]->SetRotate({ -0.5f, 0.4f, -progress * 2.1f });
        it->rings[1]->Update();

        it->rings[2]->SetScale({ ringScale * 0.82f, ringScale * 0.82f, ringScale * 0.82f });
        it->rings[2]->SetRotate({ 0.2f, -0.65f, progress * 1.8f });
        it->rings[2]->Update();

        it->cylinder->SetTranslate({
            it->position.x,
            it->position.y - cylinderHeight * 0.75f,
            it->position.z
        });
        it->cylinder->SetScale({ cylinderRadius, cylinderHeight, cylinderRadius });
        it->cylinder->SetRotate({ 0.0f, progress * 1.5f, 0.0f });
        it->cylinder->Update();

        ++it;
    }
}

void ExplosionManager::Draw() {
    for (const Explosion &explosion : explosions_) {
        for (const auto &ring : explosion.rings) {
            ring->Draw();
        }
        explosion.cylinder->Draw();
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
