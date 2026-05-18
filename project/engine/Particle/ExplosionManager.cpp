#include "ExplosionManager.h"
#include <fstream>
#include <sstream>
#include <iomanip>

void ExplosionManager::Initialize(ParticleManager *particleManager) {
    particleManager_ = particleManager;
    explosions_.clear();

    // アプリ起動時に前回の設定があれば読み込む
    LoadFromJson("resources/explosionConfig.json");
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
    }
}

void ExplosionManager::Update() {
    
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