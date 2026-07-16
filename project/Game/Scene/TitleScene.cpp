#include "TitleScene.h"
#include "3D/Object3dCommon.h"
#include "2D/SpriteCommon.h"
#include "engine/Input/Input.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/base/WinApp.h"
#include <Windows.h>
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

	// モデル
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// オブジェクト
	objA = std::make_unique<Object3d>();
	objA->Initialize(Object3dCommon::GetInstance());
	objA->SetModel("plane.obj");
	objA->transform.translate = { -2.0f,0.0f,0.0f };
	objects.push_back(objA.get());
}

void TitleScene::Finalize() {

}

void TitleScene::Update() {

	// スペースキーが押されたらシーンを切り替える
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}
	if (Input::GetInstance()->TriggerKey(DIK_S)) {
		LaunchSimulationExecutable();
	}

	// カメラの更新
	camera->Update();
	
	if (titleSprite) {
		float width = static_cast<float>(WinApp::GetClientWidth());
		float height = static_cast<float>(WinApp::GetClientHeight());
		titleSprite->SetPosition({ width / 2.0f, height / 2.0f });
		titleSprite->SetSize({ width, height });
		titleSprite->Update();
	}

	for (Object3d *object3d : objects) {
		object3d->Update();
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
	if (titleSprite) {
		titleSprite->Draw();
	}
}
