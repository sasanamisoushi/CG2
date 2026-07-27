#include "LoadingScene.h"

#include "2D/SpriteCommon.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Scene/SceneManager.h"
#include "engine/base/WinApp.h"
#include <algorithm>
#include <chrono>

void LoadingScene::Initialize() {
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	backgroundSprite_ = std::make_unique<Sprite>();
	backgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/loading_background.png");
	backgroundSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	progressSprite_ = std::make_unique<Sprite>();
	progressSprite_->Initialize(SpriteCommon::GetInstance(), "resources/loading_progress_sheet.png");
	progressSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	progressSprite_->SetTextureSize({ kFrameSize, kFrameSize });

	startTime_ = Clock::now();
	currentFrame_ = 0;
}

void LoadingScene::Finalize() {
	backgroundSprite_.reset();
	progressSprite_.reset();
}

void LoadingScene::Update() {
	const float width = static_cast<float>(WinApp::GetClientWidth());
	const float height = static_cast<float>(WinApp::GetClientHeight());
	const float elapsedSeconds = std::chrono::duration<float>(Clock::now() - startTime_).count();
	const float progress = std::clamp(elapsedSeconds / kFillDurationSeconds, 0.0f, 1.0f);

	currentFrame_ = (std::min)(
		static_cast<int>(progress * static_cast<float>(kFrameCount)),
		kFrameCount - 1);

	if (backgroundSprite_) {
		backgroundSprite_->SetPosition({ width * 0.5f, height * 0.5f });
		backgroundSprite_->SetSize({ width, height });
		backgroundSprite_->Update();
	}

	if (progressSprite_) {
		const float indicatorSize = (std::min)(width, height) * 0.52f;
		progressSprite_->SetPosition({ width * 0.5f, height * 0.58f });
		progressSprite_->SetSize({ indicatorSize, indicatorSize });
		progressSprite_->SetTextureLeftTop({ kFrameSize * static_cast<float>(currentFrame_), 0.0f });
		progressSprite_->SetTextureSize({ kFrameSize, kFrameSize });
		progressSprite_->Update();
	}

	if (elapsedSeconds >= kFillDurationSeconds + kCompletedHoldSeconds) {
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}
}

void LoadingScene::Draw() {
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	if (backgroundSprite_) {
		backgroundSprite_->Draw();
	}
	if (progressSprite_) {
		progressSprite_->Draw();
	}
}
