#pragma once
#include "engine/Camera/Camera.h"
#include "3D/Object3d.h"
#include "Game/base/BaseScene.h"
#include "2D/Sprite.h"
#include <array>
#include <memory>

class TitleScene : public BaseScene {
public:
	void Initialize() override;

	void Finalize() override;

	void Update() override;

	void Draw() override;
private:
	enum class MenuItem {
		Start,
		Settings,
		Exit,
		Count
	};
	enum class SettingsItem {
		MasterVolume,
		MouseSensitivity,
		ControlGuide,
		Back,
		Count
	};

	void DrawMenuOverlay(float screenWidth, float screenHeight);
	void UpdateSettingsInput();
	std::unique_ptr<Camera> camera;

	std::unique_ptr<Sprite> titleSprite;
	std::unique_ptr<Sprite> menuPanelSprite_;
	std::unique_ptr<Sprite> menuRowSprite_;
	std::array<std::unique_ptr<Sprite>, 8> menuLabelSprites_;
	std::array<std::unique_ptr<Sprite>, 40> settingMeterSegments_;
	std::array<std::array<std::unique_ptr<Sprite>, 5>, 2> settingValueDigitSprites_;

	// モデル
	std::vector<Object3d *> objects;
	std::unique_ptr<Object3d> objA;
	MenuItem selectedMenuItem_ = MenuItem::Start;
	SettingsItem selectedSettingsItem_ = SettingsItem::MasterVolume;
	bool isSettingsOpen_ = false;

};



