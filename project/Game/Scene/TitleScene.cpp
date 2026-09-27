#include "TitleScene.h"
#include "3D/Object3dCommon.h"
#include "2D/SpriteCommon.h"
#include "engine/Input/Input.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Audio/AudioManager.h"
#include "engine/base/WinApp.h"
#include "Game/base/GameSettings.h"
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <shellapi.h>

namespace {
	bool LaunchSimulationExecutable() {
		wchar_t modulePath[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (length == 0) {
			return false;
		}

		const std::filesystem::path currentExe(modulePath);
		const std::filesystem::path simulationExe = currentExe.parent_path() / L"CG2Simulation.exe";
		const std::filesystem::path launchExe = std::filesystem::exists(simulationExe) ? simulationExe : currentExe;
		const wchar_t *parameters = (launchExe == currentExe) ? L"--simulation" : nullptr;
		const std::filesystem::path workDir = std::filesystem::current_path();

		HINSTANCE result = ShellExecuteW(
			nullptr,
			L"open",
			launchExe.c_str(),
			parameters,
			workDir.c_str(),
			SW_SHOWNORMAL);
		return reinterpret_cast<intptr_t>(result) > 32;
	}
}

void TitleScene::Initialize() {
	// ポストエフェクトを通常状態にクリアする
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	// カメラ・シーンリソース
	camera = std::make_unique<Camera>();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	titleSprite = std::make_unique<Sprite>();
	titleSprite->Initialize(SpriteCommon::GetInstance(), "resources/title.png");
	titleSprite->SetPosition({ 640.0f, 360.0f });
	titleSprite->SetAnchorPoint({ 0.5f, 0.5f });
	titleSprite->SetSize({ 1280.0f, 720.0f });

	// タイトルロゴの書体に合わせて作成したメニュー文字スプライト。
	menuPanelSprite_ = std::make_unique<Sprite>();
	menuPanelSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	menuRowSprite_ = std::make_unique<Sprite>();
	menuRowSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	for (auto& label : menuLabelSprites_) {
		label = std::make_unique<Sprite>();
		label->Initialize(SpriteCommon::GetInstance(), "resources/title_menu_labels.png");
	}
	for (auto& segment : settingMeterSegments_) {
		segment = std::make_unique<Sprite>();
		segment->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	}
	for (auto& digitRow : settingValueDigitSprites_) {
		for (auto& digit : digitRow) {
			digit = std::make_unique<Sprite>();
			digit->Initialize(SpriteCommon::GetInstance(), "resources/hud_digits.png");
		}
	}

	// モデル
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// オブジェクト
	objA = std::make_unique<Object3d>();
	objA->Initialize(Object3dCommon::GetInstance());
	objA->SetModel("plane.obj");
	objA->transform.translate = { -2.0f,0.0f,0.0f };
	objects.push_back(objA.get());

	selectedMenuItem_ = MenuItem::Start;
	selectedSettingsItem_ = SettingsItem::MasterVolume;
	isSettingsOpen_ = false;
	AudioManager::GetInstance()->SetMasterVolume(GameSettings::GetInstance().GetMasterVolume());
}

void TitleScene::Finalize() {

}

void TitleScene::Update() {
	Input* input = Input::GetInstance();
	if (isSettingsOpen_) {
		UpdateSettingsInput();
	} else {
		const int itemCount = static_cast<int>(MenuItem::Count);
		int selection = static_cast<int>(selectedMenuItem_);
		if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W)) {
			selection = (selection + itemCount - 1) % itemCount;
		} else if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S)) {
			selection = (selection + 1) % itemCount;
		}
		selectedMenuItem_ = static_cast<MenuItem>(selection);

		if (input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE)) {
			switch (selectedMenuItem_) {
			case MenuItem::Start:
				SceneManager::GetInstance()->ChangeScene("LOADING");
				return;
			case MenuItem::Settings:
				isSettingsOpen_ = true;
				selectedSettingsItem_ = SettingsItem::MasterVolume;
				break;
			case MenuItem::Exit:
				PostQuitMessage(0);
				return;
			default:
				break;
			}
		}
	}

	// カメラの更新
	camera->Update();
	
	if (titleSprite) {
		float width = static_cast<float>(WinApp::GetClientWidth());
		float height = static_cast<float>(WinApp::GetClientHeight());
		// メインメニューはロゴの下に置けるよう、タイトル画像を上側へ収める。
		const float titleScale = isSettingsOpen_ ? 1.0f : 0.74f;
		const float titleCenterY = isSettingsOpen_ ? height * 0.5f : height * 0.37f;
		titleSprite->SetPosition({ width * 0.5f, titleCenterY });
		titleSprite->SetSize({ width * titleScale, height * titleScale });
		titleSprite->Update();
	}

	for (Object3d *object3d : objects) {
		object3d->Update();
	}
}

void TitleScene::UpdateSettingsInput() {
	Input* input = Input::GetInstance();
	if (input->TriggerKey(DIK_ESCAPE)) {
		isSettingsOpen_ = false;
		return;
	}

	const int itemCount = static_cast<int>(SettingsItem::Count);
	int selection = static_cast<int>(selectedSettingsItem_);
	if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W)) {
		selection = (selection + itemCount - 1) % itemCount;
	} else if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S)) {
		selection = (selection + 1) % itemCount;
	}
	selectedSettingsItem_ = static_cast<SettingsItem>(selection);

	const bool decrease = input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_A);
	const bool increase = input->TriggerKey(DIK_RIGHT) || input->TriggerKey(DIK_D);
	GameSettings& settings = GameSettings::GetInstance();
	if (selectedSettingsItem_ == SettingsItem::MasterVolume && (decrease || increase)) {
		settings.SetMasterVolume(settings.GetMasterVolume() + (increase ? 0.05f : -0.05f));
		AudioManager::GetInstance()->SetMasterVolume(settings.GetMasterVolume());
	} else if (selectedSettingsItem_ == SettingsItem::MouseSensitivity && (decrease || increase)) {
		settings.SetMouseSensitivity(settings.GetMouseSensitivity() + (increase ? 0.0005f : -0.0005f));
	} else if (selectedSettingsItem_ == SettingsItem::ControlGuide && (decrease || increase ||
		input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE))) {
		settings.SetControlGuideVisible(!settings.IsControlGuideVisible());
	}

	if ((input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE)) && selectedSettingsItem_ == SettingsItem::Back) {
		isSettingsOpen_ = false;
	}
}

void TitleScene::Draw() {
	// 3Dオブジェクトの描画準備
	Object3dCommon::GetInstance()->SetCommonDrawSettings();
	// 3Dオブジェクトの描画
	for (Object3d *object3d : objects) {
		object3d->Draw();
	}

	SpriteCommon::GetInstance()->SetCommonPipelineState();
	// タイトル画像の外側も常に黒で塗り、画面クリア色が見えないようにする。
	if (menuPanelSprite_) {
		menuPanelSprite_->SetPosition({ 0.0f, 0.0f });
		menuPanelSprite_->SetSize({
			static_cast<float>(WinApp::GetClientWidth()),
			static_cast<float>(WinApp::GetClientHeight())
		});
		menuPanelSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		menuPanelSprite_->Update();
		menuPanelSprite_->Draw();
	}
	if (titleSprite && !isSettingsOpen_) {
		titleSprite->Draw();
	}

	DrawMenuOverlay(static_cast<float>(WinApp::GetClientWidth()), static_cast<float>(WinApp::GetClientHeight()));
}

void TitleScene::DrawMenuOverlay(float screenWidth, float screenHeight) {
	if (!menuPanelSprite_ || !menuRowSprite_) {
		return;
	}

	struct LabelRegion {
		float x;
		float y;
		float width;
		float height;
	};
	// 1214 x 1295 の title_menu_labels.png 内にある各ラベルの領域。
	constexpr std::array<LabelRegion, 8> kLabelRegions = {{
		{ 75.0f, 45.0f, 1065.0f, 140.0f }, // START GAME
		{ 170.0f, 195.0f, 880.0f, 130.0f }, // SETTINGS
		{ 145.0f, 345.0f, 930.0f, 140.0f }, // EXIT GAME
		{ 35.0f, 500.0f, 1145.0f, 140.0f }, // MASTER VOLUME
		{ 10.0f, 650.0f, 1195.0f, 140.0f }, // MOUSE SENSITIVITY
		{ 75.0f, 805.0f, 1060.0f, 140.0f }, // CONTROL GUIDE
		{ 330.0f, 960.0f, 555.0f, 135.0f }, // BACK
		{ 245.0f, 1100.0f, 725.0f, 145.0f }, // ON / OFF
	}};
	const auto drawLabel = [&](size_t labelIndex, float centerX, float topY, float targetHeight, float alpha = 1.0f) {
		if (labelIndex >= menuLabelSprites_.size() || !menuLabelSprites_[labelIndex]) {
			return;
		}
		const LabelRegion& region = kLabelRegions[labelIndex];
		const float targetWidth = targetHeight * region.width / region.height;
		Sprite* label = menuLabelSprites_[labelIndex].get();
		label->SetTextureLeftTop({ region.x, region.y });
		label->SetTextureSize({ region.width, region.height });
		label->SetPosition({ centerX - targetWidth * 0.5f, topY });
		label->SetSize({ targetWidth, targetHeight });
		label->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
		label->Update();
		label->Draw();
	};
	if (!isSettingsOpen_) {
		// メインメニューはロゴの下に文字だけを置く。選択枠や背景パネルは描かない。
		const auto drawMainMenuLabel = [&](size_t labelIndex, MenuItem item, float topY) {
			const bool selected = selectedMenuItem_ == item;
			const float labelHeight = selected ? 52.0f : 44.0f;
			drawLabel(labelIndex, screenWidth * 0.5f,
				topY - (labelHeight - 44.0f) * 0.5f, labelHeight,
				selected ? 1.0f : 0.62f);
		};
		drawMainMenuLabel(0, MenuItem::Start, screenHeight - 180.0f);
		drawMainMenuLabel(1, MenuItem::Settings, screenHeight - 123.0f);
		drawMainMenuLabel(2, MenuItem::Exit, screenHeight - 66.0f);
		return;
	}

	const GameSettings& settings = GameSettings::GetInstance();
	// 設定は黒背景へ直接表示する。選択枠・パネルは使わず、文字の強調で選択状態を示す。
	drawLabel(1, screenWidth * 0.5f, screenHeight * 0.10f, 60.0f);
	const float rowStartY = screenHeight * 0.27f;
	const float rowSpacing = 82.0f;
	const float labelCenterX = screenWidth * 0.32f;
	const float sliderX = screenWidth * 0.56f;
	const float sliderWidth = screenWidth * 0.30f;

	const auto drawSettingLabel = [&](int index, size_t labelIndex, float topY) {
		const bool selected = index == static_cast<int>(selectedSettingsItem_);
		const float labelHeight = selected ? 54.0f : 46.0f;
		drawLabel(labelIndex, labelCenterX, topY - (labelHeight - 46.0f) * 0.5f,
			labelHeight, selected ? 1.0f : 0.58f);
	};
	const auto drawLevelMeter = [&](int meterIndex, float topY, float valueRatio,
		int currentStep, int maxStep, bool selected) {
		constexpr int kSegmentCount = 20;
		const float ratio = std::clamp(valueRatio, 0.0f, 1.0f);
		const int litSegmentCount = static_cast<int>(ratio * static_cast<float>(kSegmentCount) + 0.5f);
		const float segmentGap = 4.0f;
		const float segmentWidth = (sliderWidth - segmentGap * static_cast<float>(kSegmentCount - 1)) /
			static_cast<float>(kSegmentCount);
		const float segmentY = topY + 17.0f;
		for (int index = 0; index < kSegmentCount; ++index) {
			Sprite* segment = settingMeterSegments_[meterIndex * kSegmentCount + index].get();
			segment->SetPosition({ sliderX + static_cast<float>(index) * (segmentWidth + segmentGap), segmentY });
			segment->SetSize({ segmentWidth, 20.0f });
			const bool isLit = index < litSegmentCount;
			const bool isCurrent = isLit && index == litSegmentCount - 1;
			segment->SetColor(isCurrent
				? Vector4{ 0.92f, 0.98f, 1.0f, 1.0f }
				: (isLit
					? (selected ? Vector4{ 1.0f, 0.46f, 0.08f, 1.0f } : Vector4{ 0.58f, 0.27f, 0.07f, 0.92f })
					: Vector4{ 0.26f, 0.29f, 0.32f, 1.0f }));
			segment->Update();
			segment->Draw();
		}

		const std::string levelText = std::to_string(currentStep) + "/" + std::to_string(maxStep);
		const float digitWidth = 20.0f;
		const float digitHeight = 26.0f;
		const float digitStartX = sliderX + sliderWidth + 18.0f;
		for (size_t index = 0; index < levelText.size() && index < settingValueDigitSprites_[meterIndex].size(); ++index) {
			Sprite* digit = settingValueDigitSprites_[meterIndex][index].get();
			const int glyphIndex = levelText[index] == '/' ? 10 : levelText[index] - '0';
			digit->SetTextureLeftTop({ static_cast<float>(glyphIndex * 64), 0.0f });
			digit->SetTextureSize({ 64.0f, 80.0f });
			digit->SetPosition({ digitStartX + digitWidth * static_cast<float>(index), topY + 13.0f });
			digit->SetSize({ digitWidth, digitHeight });
			digit->SetColor(selected ? Vector4{ 1.0f, 0.82f, 0.24f, 1.0f } : Vector4{ 0.62f, 0.66f, 0.70f, 0.86f });
			digit->Update();
			digit->Draw();
		}
	};

	const int volumeStep = static_cast<int>(settings.GetMasterVolume() * 20.0f + 0.5f);
	const float sensitivityRatio = (settings.GetMouseSensitivity() - 0.0005f) / 0.0095f;
	const int sensitivityStep = static_cast<int>(sensitivityRatio * 19.0f + 0.5f);
	drawSettingLabel(0, 3, rowStartY);
	drawLevelMeter(0, rowStartY, settings.GetMasterVolume(), volumeStep, 20,
		selectedSettingsItem_ == SettingsItem::MasterVolume);
	drawSettingLabel(1, 4, rowStartY + rowSpacing);
	drawLevelMeter(1, rowStartY + rowSpacing, sensitivityRatio, sensitivityStep, 19,
		selectedSettingsItem_ == SettingsItem::MouseSensitivity);
	drawSettingLabel(2, 5, rowStartY + rowSpacing * 2.0f);
	const bool guideSelected = selectedSettingsItem_ == SettingsItem::ControlGuide;
	drawLabel(7, screenWidth * 0.75f, rowStartY + rowSpacing * 2.0f + 3.0f,
		guideSelected ? 42.0f : 36.0f,
		guideSelected ? 1.0f : (settings.IsControlGuideVisible() ? 0.74f : 0.40f));
	const bool backSelected = selectedSettingsItem_ == SettingsItem::Back;
	drawLabel(6, screenWidth * 0.5f, rowStartY + rowSpacing * 3.0f,
		backSelected ? 54.0f : 46.0f, backSelected ? 1.0f : 0.58f);
}
