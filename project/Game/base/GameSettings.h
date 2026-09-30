#pragma once

#include <algorithm>
#include <fstream>
#include <string>

// タイトル・ゲームプレイ間で共有する、実行中のゲーム設定。
class GameSettings final {
public:
	static GameSettings& GetInstance() {
		static GameSettings instance;
		return instance;
	}

	float GetMasterVolume() const { return masterVolume_; }
	void SetMasterVolume(float volume) { masterVolume_ = std::clamp(volume, 0.0f, 1.0f); }

	float GetMouseSensitivity() const { return mouseSensitivity_; }
	void SetMouseSensitivity(float sensitivity) { mouseSensitivity_ = std::clamp(sensitivity, 0.0005f, 0.0100f); }

	bool IsControlGuideVisible() const { return isControlGuideVisible_; }
	void SetControlGuideVisible(bool visible) { isControlGuideVisible_ = visible; }

	// タイトルから本編へ入る際のフォールド画面遷移。変更時に即座に保存する。
	bool IsFoldTransitionEnabled() const { return isFoldTransitionEnabled_; }
	void SetFoldTransitionEnabled(bool enabled) {
		isFoldTransitionEnabled_ = enabled;
		Save();
	}

private:
	GameSettings() { Load(); }

	void Load() {
		std::ifstream input(kSettingsPath);
		if (!input.is_open()) {
			return;
		}
		std::string name;
		int value = 0;
		while (input >> name >> value) {
			if (name == "fold_transition_enabled") {
				isFoldTransitionEnabled_ = value != 0;
			}
		}
	}

	void Save() const {
		std::ofstream output(kSettingsPath, std::ios::trunc);
		if (output.is_open()) {
			output << "fold_transition_enabled " << (isFoldTransitionEnabled_ ? 1 : 0) << '\n';
		}
	}

	static constexpr const char* kSettingsPath = "resources/game_settings.cfg";
	float masterVolume_ = 1.0f;
	float mouseSensitivity_ = 0.0020f;
	bool isControlGuideVisible_ = true;
	bool isFoldTransitionEnabled_ = true;
};
