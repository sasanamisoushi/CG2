#pragma once

#include "2D/Sprite.h"
#include "Game/base/BaseScene.h"
#include <chrono>
#include <memory>

class LoadingScene : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	using Clock = std::chrono::steady_clock;

	static constexpr int kFrameCount = 8;
	static constexpr float kFrameSize = 512.0f;
	static constexpr float kFillDurationSeconds = 1.6f;
	static constexpr float kCompletedHoldSeconds = 0.25f;

	std::unique_ptr<Sprite> backgroundSprite_;
	std::unique_ptr<Sprite> progressSprite_;
	Clock::time_point startTime_{};
	int currentFrame_ = 0;
};
