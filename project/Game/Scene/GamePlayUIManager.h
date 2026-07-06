#pragma once

#include <string>
#include <vector>

// ゲームプレイUIおよびシミュレーションツールのUI描画を管理するクラス
class GamePlayUIManager {
public:
	GamePlayUIManager();
	~GamePlayUIManager() = default;

	void Initialize();
	void UpdateUI();

	void SetSimulationMode(bool isSimulation) { isSimulationMode_ = isSimulation; }

private:
	void DrawSimulationScreenUI();
	void DrawSimulationSaveControls();
	void DrawGameplayActionControls();
	void DrawMissileSettingsUI();

	bool isSimulationMode_ = false;

	// UI・シミュレーション関連の変数
	bool showSimulationWindow_ = false;
	int currentSimulationTarget_ = 0;
	std::string simulationSaveMessage_;
	char simulationActionName_[64] = "Action1";
	std::vector<std::string> simulationActionNames_;
	int selectedSimulationActionIndex_ = 0;
	std::string simulationActionMessage_;
	int simulationPlaybackMode_ = 0;
	char missilePresetName_[64] = "MissilePreset1";
	int missilePresetTypeIndex_ = 0;
	std::vector<std::string> missilePresetNames_[2];
	int selectedMissilePresetIndex_[2] = { 0, 0 };
	std::string missilePresetMessage_;
};
