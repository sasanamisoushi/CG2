#pragma once
#include "engine/math/MyMath.h"
#include "engine/Utility/StageLoader.h"
#include <string>
#include <list>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>


class Enemy;
class Player;
class Obstacle;

class EditorReceiver {
public:
	// どこからでも呼べるようにシングルトン（1つだけの存在）にする
	static EditorReceiver *GetInstance();

	void Initialize();
	void Finalize();

	// 毎フレーム呼んで、Blenderから最新のデータが届いていたら敵を更新する
	bool Update(Player *player, std::list<std::unique_ptr<Enemy>> &enemies, std::list<std::unique_ptr<Obstacle>> &obstacles, std::vector<EnemySpawnData> &enemySpawns);

private:
	EditorReceiver() = default;
	~EditorReceiver() = default;

	// 裏でずっと通信を待ち受ける専用の処理（別スレッド）
	void ReceiveLoop();

	std::thread receiveThread_;
	std::atomic<bool> isRunning_ = false;
	std::mutex dataMutex_; // データの衝突を防ぐ鍵

	std::string latestJsonData_; // 届いた最新のJSON文字列
	std::string lastAppliedJsonData_;
	bool hasNewData_ = false;    // 新しいデータが届いたかどうかのフラグ
};

