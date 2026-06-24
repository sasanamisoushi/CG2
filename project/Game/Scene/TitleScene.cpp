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
	// 繝昴せ繝医お繝輔ぉ繧ｯ繝医ｒ騾壼ｸｸ迥ｶ諷九↓繧ｯ繝ｪ繧｢縺吶ｋ
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	//繧ｫ繝｡繝ｩ繝ｻ繧ｷ繝ｼ繝ｳ繝ｪ繧ｽ繝ｼ繧ｹ
	camera = std::make_unique<Camera>();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	titleSprite = std::make_unique<Sprite>();
	titleSprite->Initialize(SpriteCommon::GetInstance(), "resources/title.png");
	titleSprite->SetPosition({ 640.0f, 360.0f });
	titleSprite->SetAnchorPoint({ 0.5f, 0.5f });
	titleSprite->SetSize({ 1280.0f, 720.0f });

	//繝｢繝・Ν
	ModelManager::GetInstance()->LoadModel("plane.obj");

	//繧ｪ繝悶ず繧ｧ繧ｯ繝・
	objA = std::make_unique<Object3d>();
	objA->Initialize(Object3dCommon::GetInstance());
	objA->SetModel("plane.obj");
	objA->transform.translate = { -2.0f,0.0f,0.0f };
	objects.push_back(objA.get());
}

void TitleScene::Finalize() {

}

void TitleScene::Update() {

	//繧ｹ繝壹・繧ｹ繧ｭ繝ｼ縺梧款縺輔ｌ縺溘ｉ繧ｷ繝ｼ繝ｳ繧貞・繧頑崛縺医ｋ
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}
	if (Input::GetInstance()->TriggerKey(DIK_S)) {
		LaunchSimulationExecutable();
	}

	//繧ｫ繝｡繝ｩ縺ｮ譖ｴ譁ｰ
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
	//3D繧ｪ繝悶ず繧ｧ繝医・謠冗判貅門ｙ
	Object3dCommon::GetInstance()->SetCommonDrawSettings();
	//3D繧ｪ繝悶ず繧ｧ繧ｯ繝医・謠冗判
	for (Object3d *object3d : objects) {
		object3d->Draw();
	}

	SpriteCommon::GetInstance()->SetCommonPipelineState();
	if (titleSprite) {
		titleSprite->Draw();
	}
}
