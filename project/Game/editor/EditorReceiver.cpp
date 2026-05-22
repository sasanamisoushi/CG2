#define WIN32_LEAN_AND_MEAN // ← これが「古い辞書を呼ばない」魔法の呪文です
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include "EditorReceiver.h"
#include "externals/json.hpp"
#include "Game/enemy/Enemy.h"
#include "Game/Player/Player.h"
#include "Game/obstacle/Obstacle.h"


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

void EditorReceiver::Update(Player *player, std::list<std::unique_ptr<Enemy>> &enemies, std::list<std::unique_ptr<Obstacle>> &obstacles, std::vector<Vector3> &enemySpawnPoints) {
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
			obstacles.clear();
			enemySpawnPoints.clear();

			for (auto &objData : root["objects"]) {
				// ==========================================
				// 1. 座標・スケール・回転 の取得
				// ==========================================
				auto &trans = objData["transform"]["translation"];
				Vector3 position = { trans[0].get<float>(), trans[2].get<float>(), trans[1].get<float>() };

				auto &scaleData = objData["transform"]["scale"];
				Vector3 scale = { scaleData[0].get<float>(), scaleData[2].get<float>(), scaleData[1].get<float>() };

				// ★追加：回転データを Degree(度) から Radian(弧度) に変換して読み込む！
				Vector3 rotation = { 0.0f, 0.0f, 0.0f };
				if (objData["transform"].contains("rotation")) {
					auto &rotData = objData["transform"]["rotation"];
					float toRad = 3.141592654f / 180.0f; // 変換用の魔法の数字
					rotation = {
						rotData[0].get<float>() * toRad,
						rotData[2].get<float>() * toRad,
						rotData[1].get<float>() * toRad
					};
				}

				// ==========================================
				// 2. 種類(category)ごとの生成処理
				// ==========================================
				if (objData.contains("category")) {
					std::string category = objData["category"].get<std::string>();

					if (category == "PLAYER") {
						// プレイヤーの座標を上書き
						if (player) {
							// ※Playerクラスに SetPosition などがあればここで呼ぶ
							player->SetPosition(position);
							player->SetRotation(rotation);
						}
					} else if (category == "ENEMY") {
						enemySpawnPoints.push_back(position);
						auto newEnemy = std::make_unique<Enemy>();
						newEnemy->Initialize(position);
						newEnemy->SetRotation(rotation);
						enemies.push_back(std::move(newEnemy));
					} else if (category == "OBSTACLE") {
						// Blenderのオブジェクト名をモデル名として使う
						std::string modelName = "ObstacleBox";
						if (objData.contains("name")) {
							std::string objName = objData["name"].get<std::string>();
							
							// Blenderで名前が重複した際につくサフィックス（.001など）を切り取る
							size_t dotPos = objName.find('.');
							if (dotPos != std::string::npos) {
								modelName = objName.substr(0, dotPos);
							} else {
								modelName = objName;
							}
						}

						auto newObstacle = std::make_unique<Obstacle>();
						// さっき作ってくれた完璧なInitialize関数！
						newObstacle->Initialize(modelName, position, rotation, scale);
						obstacles.push_back(std::move(newObstacle));
					}
				}
				// 互換性用（Blenderで種類を設定し忘れた時のための予備）
				else if (objData.contains("enemy")) {
					enemySpawnPoints.push_back(position);
					auto newEnemy = std::make_unique<Enemy>();
					newEnemy->Initialize(position);
					newEnemy->SetRotation(rotation);
					enemies.push_back(std::move(newEnemy));
				}
				else if (objData.contains("type") && objData["type"].get<std::string>() == "MESH") {
					std::string modelName = "ObstacleBox";
					if (objData.contains("name")) {
						std::string objName = objData["name"].get<std::string>();
						
						// Blenderで名前が重複した際につくサフィックス（.001など）を切り取る
						size_t dotPos = objName.find('.');
						if (dotPos != std::string::npos) {
							modelName = objName.substr(0, dotPos);
						} else {
							modelName = objName;
						}
					}

					auto newObstacle = std::make_unique<Obstacle>();
					newObstacle->Initialize(modelName, position, rotation, scale);
					obstacles.push_back(std::move(newObstacle));
				}
			}
		}
	} catch (...) {
		// もし通信が途切れたり変なデータが来ても無視してゲームを落とさない
	}
}
