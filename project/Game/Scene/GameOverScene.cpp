#include "GameOverScene.h"
#include "2D/SpriteCommon.h"
#include "engine/Input/Input.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Graphics/PostEffect.h"
#include "3D/Object3dCommon.h"

void GameOverScene::Initialize() {
	// カメラの生成と初期化
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
	camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

	// ポストエフェクトをグレースケール状態にする（演出の継続）
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetVignetteSmoothing(0.42f, 0.42f, 3.0f);
	}

	// ゲームオーバー表示スプライトの生成と初期化
	gameOverSprite_ = std::make_unique<Sprite>();
	gameOverSprite_->Initialize(SpriteCommon::GetInstance(), "resources/gameover.png");
	gameOverSprite_->SetPosition({ 640.0f, 360.0f });
	gameOverSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	gameOverSprite_->SetSize({ 600.0f, 600.0f });
}

void GameOverScene::Finalize() {
	// シーン終了時にポストエフェクトを通常に戻す
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}
}

void GameOverScene::Update() {
	// カメラの更新
	camera_->Update();

	// スプライトの更新
	if (gameOverSprite_) {
		gameOverSprite_->Update();
	}

	// SPACEキーが押されたらタイトルシーンへ遷移
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}

void GameOverScene::Draw() {
	// スプライトの描画
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	if (gameOverSprite_) {
		gameOverSprite_->Draw();
	}
}
