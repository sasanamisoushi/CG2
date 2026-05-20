#pragma once
#include "engine/math/MyMath.h"
#include <string>
#include <list>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>


class Enemy;

class EditorReceiver {
public:
	// どこからでも呼べるようにシングルトン（1つだけの存在）にする
	static EditorReceiver *GetInstance();

	void Initialize();
	void Finalize();

	// 毎フレーム呼んで、Blenderから最新のデータが届いていたら敵を更新する
	void Update(std::list<std::unique_ptr<Enemy>> &enemies);

private:
	EditorReceiver() = default;
	~EditorReceiver() = default;

	// 裏でずっと通信を待ち受ける専用の処理（別スレッド）
	void ReceiveLoop();

	std::thread receiveThread_;
	std::atomic<bool> isRunning_ = false;
	std::mutex dataMutex_; // データの衝突を防ぐ鍵

	std::string latestJsonData_; // 届いた最新のJSON文字列
	bool hasNewData_ = false;    // 新しいデータが届いたかどうかのフラグ
};

