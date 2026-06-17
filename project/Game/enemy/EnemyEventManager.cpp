#include "EnemyEventManager.h"
#include "externals/json.hpp"
#include <fstream>
#include <Windows.h>

using json = nlohmann::json;

void EnemyEventManager::LoadEvents(const std::string& filepath) {
    events_.clear();

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        // ファイルが無ければ新規作成扱いとするため、何もせずリターン
        return;
    }

    json root;
    try {
        ifs >> root;
    } catch (const json::parse_error& e) {
        OutputDebugStringA((" [EnemyEventManager] JSON Parse error: " + std::string(e.what()) + "\n").c_str());
        return;
    }

    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& evData : root["events"]) {
            EnemyEvent ev;
            ev.triggerEnemyName = evData.value("trigger", "");
            ev.targetEnemyName = evData.value("target", "");
            ev.delayFrames = evData.value("delay", 0);

            if (!ev.triggerEnemyName.empty() && !ev.targetEnemyName.empty()) {
                events_.push_back(ev);
            }
        }
    }
}

void EnemyEventManager::SaveEvents(const std::string& filepath) {
    json root;
    json eventsArray = json::array();

    for (const auto& ev : events_) {
        json evData;
        evData["trigger"] = ev.triggerEnemyName;
        evData["target"] = ev.targetEnemyName;
        evData["delay"] = ev.delayFrames;
        eventsArray.push_back(evData);
    }

    root["events"] = eventsArray;

    std::ofstream ofs(filepath);
    if (ofs.is_open()) {
        ofs << root.dump(4);
    } else {
        OutputDebugStringA((" [EnemyEventManager] Failed to save events to: " + filepath + "\n").c_str());
    }
}

void EnemyEventManager::AddEvent(const std::string& trigger, const std::string& target, int delayFrames) {
    if (trigger.empty() || target.empty()) return;
    
    // 重複チェック
    for (auto& ev : events_) {
        if (ev.triggerEnemyName == trigger && ev.targetEnemyName == target) {
            ev.delayFrames = delayFrames; // 更新
            return;
        }
    }

    EnemyEvent ev;
    ev.triggerEnemyName = trigger;
    ev.targetEnemyName = target;
    ev.delayFrames = delayFrames;
    events_.push_back(ev);
}

void EnemyEventManager::RemoveEvent(size_t index) {
    if (index < events_.size()) {
        events_.erase(events_.begin() + index);
    }
}

std::vector<EnemyEvent> EnemyEventManager::GetEventsForTrigger(const std::string& triggerEnemyName) const {
    std::vector<EnemyEvent> result;
    for (const auto& ev : events_) {
        if (ev.triggerEnemyName == triggerEnemyName) {
            result.push_back(ev);
        }
    }
    return result;
}

bool EnemyEventManager::IsTargetEnemy(const std::string& enemyName) const {
    if (enemyName.empty()) return false;
    for (const auto& ev : events_) {
        if (ev.targetEnemyName == enemyName) {
            return true;
        }
    }
    return false;
}
