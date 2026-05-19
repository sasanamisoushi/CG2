#pragma once
#include <string>
#include <list>
#include <memory>

class Enemy;

class StageLoader {
public:
	// Blenderから出力したJSONを読み込み、敵リストに追加する
	static bool LoadSceneJson(const std::string &filePath, std::list<std::unique_ptr<Enemy>> &enemies);
};

