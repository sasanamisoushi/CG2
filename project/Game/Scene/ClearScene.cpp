#include "ClearScene.h"
#include "2D/SpriteCommon.h"
#include "engine/Input/Input.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Graphics/PostEffect.h"
#include "3D/Object3dCommon.h"
#include "engine/base/WinApp.h"

void ClearScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
	camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	clearSprite_ = std::make_unique<Sprite>();
	clearSprite_->Initialize(SpriteCommon::GetInstance(), "resources/clear.png");
	clearSprite_->SetPosition({ 640.0f, 360.0f });
	clearSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	clearSprite_->SetSize({ 1280.0f, 720.0f });
}

void ClearScene::Finalize() {
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}
}

void ClearScene::Update() {
	camera_->Update();

	if (clearSprite_) {
		float width = static_cast<float>(WinApp::GetClientWidth());
		float height = static_cast<float>(WinApp::GetClientHeight());
		clearSprite_->SetPosition({ width / 2.0f, height / 2.0f });
		clearSprite_->SetSize({ width, height });
		clearSprite_->Update();
	}

	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}

void ClearScene::Draw() {
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	if (clearSprite_) {
		clearSprite_->Draw();
	}
}
