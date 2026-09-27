#pragma once

#include <algorithm>

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

private:
	GameSettings() = default;
	float masterVolume_ = 1.0f;
	float mouseSensitivity_ = 0.0020f;
	bool isControlGuideVisible_ = true;
};
