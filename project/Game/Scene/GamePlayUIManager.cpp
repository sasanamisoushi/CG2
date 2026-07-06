#include "GamePlayUIManager.h"
#include "engine/Debug/ImGuiManager.h"
#include <externals/imgui/imgui.h>

GamePlayUIManager::GamePlayUIManager() {
}

void GamePlayUIManager::Initialize() {
}

void GamePlayUIManager::UpdateUI() {
#ifdef ENABLE_IMGUI
	if (ImGuiManager::IsVisible()) {
		if (isSimulationMode_) {
			DrawSimulationScreenUI();
			return;
		}

		ImGui::Begin("Simulation");
		ImGui::Text("シミュレーション設定");
		ImGui::TextWrapped("保存済み設定の読み込みはここで行えます。細かい保存や確認は専用画面を開いてください。");
		DrawGameplayActionControls();
		ImGui::End();
	}
#endif
}

void GamePlayUIManager::DrawSimulationScreenUI() {
#ifdef ENABLE_IMGUI
	ImGui::Begin("Simulation Mode", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Simulation Tools");
	DrawSimulationSaveControls();
	DrawMissileSettingsUI();
	ImGui::End();
#endif
}

void GamePlayUIManager::DrawSimulationSaveControls() {
#ifdef ENABLE_IMGUI
	ImGui::Text("Save controls will be here");
#endif
}

void GamePlayUIManager::DrawGameplayActionControls() {
#ifdef ENABLE_IMGUI
	ImGui::Text("Gameplay action controls will be here");
#endif
}

void GamePlayUIManager::DrawMissileSettingsUI() {
#ifdef ENABLE_IMGUI
	ImGui::Text("Missile settings UI will be here");
#endif
}
