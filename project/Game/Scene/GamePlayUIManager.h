#pragma once

#include <string>
#include <vector>

class GamePlayScene;

// 繧ｲ繝ｼ繝繝励Ξ繧､UI縺翫ｈ縺ｳ繧ｷ繝溘Η繝ｬ繝ｼ繧ｷ繝ｧ繝ｳ繝・・繝ｫ縺ｮUI謠冗判繧堤ｮ｡逅・☆繧九け繝ｩ繧ｹ
class GamePlayUIManager {
public:
	GamePlayUIManager(GamePlayScene* scene);
	~GamePlayUIManager() = default;

	void Initialize();
	void UpdateUI();

private:
	GamePlayScene* scene_ = nullptr;
};