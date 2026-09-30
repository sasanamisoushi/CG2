#pragma once

#include "engine/math/MyMath.h"

// タイトルの出撃演出からゲームプレイ開始へ渡す最小限の状態。
// シーン自体は切り替えるが、進行方向と初速を合わせて連続して見せる。
struct GameStartTransitionData {
	Vector3 forward = { 0.0f, 0.0f, 1.0f };
	float initialSpeed = 0.0f;
	int boostFrames = 0;
};

class GameStartTransition {
public:
	static void Begin(const GameStartTransitionData& transition) {
		Data() = transition;
		IsPending() = true;
	}

	static bool Consume(GameStartTransitionData& transition) {
		if (!IsPending()) {
			return false;
		}
		transition = Data();
		IsPending() = false;
		return true;
	}

private:
	static GameStartTransitionData& Data() {
		static GameStartTransitionData data{};
		return data;
	}

	static bool& IsPending() {
		static bool pending = false;
		return pending;
	}
};
