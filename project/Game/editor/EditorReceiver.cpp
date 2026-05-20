#define WIN32_LEAN_AND_MEAN // ← これが「古い辞書を呼ばない」魔法の呪文です
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include "EditorReceiver.h"
#include "externals/json.hpp"
#include "Game/enemy/Enemy.h"


// Windowsの通信用ライブラリを自動でリンクする
#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;

EditorReceiver *EditorReceiver::GetInstance() {
	static EditorReceiver instance;
	return &instance;
}

void EditorReceiver::Initialize() {
	isRunning_ = true;
	receiveThread_ = std::thread(&EditorReceiver::ReceiveLoop, this); // 裏で受信スタート！
}

void EditorReceiver::Finalize() {
	isRunning_ = false;
	if (receiveThread_.joinable()) {
		receiveThread_.join(); // スレッドを安全に終了させる
	}
}

void EditorReceiver::ReceiveLoop() {
	// Windowsネットワークの初期化
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

	// UDPソケット（受け口）を作る
	SOCKET recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sockaddr_in recvAddr;
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(50000); // 💡ポート50000番を専用の通り道にする
	recvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(recvSocket, (SOCKADDR *)&recvAddr, sizeof(recvAddr));

	// 通信が来ていなくても処理が止まらないようにする（ノンブロッキング）
	u_long val = 1;
	ioctlsocket(recvSocket, FIONBIO, &val);

	char buffer[8192]; // 最大8KBまで受け取る

	while (isRunning_) {
		sockaddr_in senderAddr;
		int senderAddrSize = sizeof(senderAddr);
		int bytesRecv = recvfrom(recvSocket, buffer, sizeof(buffer) - 1, 0, (SOCKADDR *)&senderAddr, &senderAddrSize);

		if (bytesRecv > 0) {
			buffer[bytesRecv] = '\0'; // 文字列の終わりにフタをする

			// 受け取ったJSONを安全に保存！
			std::lock_guard<std::mutex> lock(dataMutex_);
			latestJsonData_ = buffer;
			hasNewData_ = true;
		} else {
			Sleep(10); // データがない場合は少し休む
		}
	}

	closesocket(recvSocket);
	WSACleanup();
}

void EditorReceiver::Update(std::list<std::unique_ptr<Enemy>> &enemies) {
	std::string jsonStr;
	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		if (!hasNewData_) return; // 💡新しいデータが無ければ何もしない（一瞬で終わる）
		jsonStr = latestJsonData_;
		hasNewData_ = false;
	}

	// 新しいデータが届いていたら、ゲーム内の敵を一気に書き換える！
	try {
		json root = json::parse(jsonStr);
		if (root.contains("objects") && root["objects"].is_array()) {

			enemies.clear(); // 一旦リセットして再配置（エディタモード用のシンプルな同期）

			for (auto &objData : root["objects"]) {
				if (objData.contains("enemy")) {
					auto &trans = objData["transform"]["translation"];
					Vector3 position = { trans[0].get<float>(), trans[1].get<float>(), trans[2].get<float>() };

					auto &scaleData = objData["transform"]["scale"];
					Vector3 scale = { scaleData[0].get<float>(), scaleData[1].get<float>(), scaleData[2].get<float>() };

					auto newEnemy = std::make_unique<Enemy>();
					newEnemy->Initialize(position);
					newEnemy->SetScale(scale);
					enemies.push_back(std::move(newEnemy));
				}
			}
		}
	} catch (...) {
		// もし通信が途切れたり変なデータが来ても無視してゲームを落とさない
	}
}