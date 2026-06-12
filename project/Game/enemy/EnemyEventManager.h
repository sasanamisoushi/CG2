#pragma once
#include <string>
#include <vector>

struct EnemyEvent {
    std::string triggerEnemyName;
    std::string targetEnemyName;
    int delayFrames = 0;
};

class EnemyEventManager {
public:
    EnemyEventManager() = default;
    ~EnemyEventManager() = default;

    // JSONからイベントを読み込む
    void LoadEvents(const std::string& filepath);

    // JSONにイベントを保存する
    void SaveEvents(const std::string& filepath);

    // 新しいイベントを追加
    void AddEvent(const std::string& trigger, const std::string& target, int delayFrames);

    // 指定されたインデックスのイベントを削除
    void RemoveEvent(size_t index);

    // イベント一覧を取得
    const std::vector<EnemyEvent>& GetEvents() const { return events_; }

    // 指定した敵が死んだときに発動するイベント（ターゲットの名前と遅延フレーム）を取得
    std::vector<EnemyEvent> GetEventsForTrigger(const std::string& triggerEnemyName) const;

    // 指定した敵がどこかのイベントのターゲット（増援）になっているか確認
    bool IsTargetEnemy(const std::string& enemyName) const;

private:
    std::vector<EnemyEvent> events_;
};
