#include "GamePlayScene.h"
#include "3D/ModelManager.h"
#include <Windows.h>
#include "engine/Graphics/DirectXCommon.h"
#include "2D/SpriteCommon.h"
#include "3D/Object3dCommon.h"
#include "engine/Input/Input.h"
#include "engine/Debug/ImGuiManager.h"
#include "engine/Resource/TextureManager.h"
#include <externals/imgui/imgui.h>
#include "engine/Camera/FlyCamera.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Utility/StageLoader.h"
#include "engine/Utility/StageValidation.h"
#include "externals/json.hpp"
#include "Game/editor/EditorReceiver.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <shellapi.h>

using json = nlohmann::json;

namespace {
	constexpr int kEnemyRespawnDelayFrames = 180;
	constexpr int kNoRespawnTimer = -1;
	constexpr float kNormalCameraFovY = 0.45f;
	constexpr float kNormalCameraFarClip = 100.0f;
	constexpr float kCinematicCameraFovY = 0.70f;
	constexpr float kCinematicCameraFarClip = 250.0f;
	constexpr float kCinematicCameraFocusBlend = 0.18f;
	constexpr float kCinematicCameraDirectionBlend = 0.10f;
	constexpr float kCinematicCameraPositionBlend = 0.10f;
	constexpr float kCinematicCameraRotationBlend = 0.12f;
	constexpr float kAimAssistScreenRadius = 96.0f;
	constexpr float kAimAssistMaxDistance = 180.0f;
	constexpr size_t kMultiLockMaxTargets = 6;
	constexpr int kMultiLockAcquireIntervalFrames = 8;
	constexpr float kMultiLockScreenRadius = 320.0f;
	constexpr float kMultiLockMaxDistance = 240.0f;
	constexpr float kRadiansToDegrees = 180.0f / 3.141592654f;
	constexpr size_t kInvalidSceneObjectIndex = static_cast<size_t>(-1);
	const char *kSimulationActionsFilePath = "resources/simulation_actions.json";
	const char *kMissilePresetsFilePath = "resources/missile_presets.json";
	const char *kPlayerModelName = "vf-15c/scene.gltf";
	const char *kLockOnReticleTexturePath = "resources/lock_on_reticle.png";
	const char *kAimCursorTexturePath = "resources/aim_cursor.png";

	Vector3 SubtractVector3(const Vector3 &lhs, const Vector3 &rhs) {
		return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
	}

	Vector3 AddVector3(const Vector3 &lhs, const Vector3 &rhs) {
		return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
	}

	Vector3 ScaleVector3(const Vector3 &value, float scale) {
		return { value.x * scale, value.y * scale, value.z * scale };
	}

	Vector3 FlattenYVector3(const Vector3 &value) {
		return { value.x, 0.0f, value.z };
	}

	std::filesystem::path GetCurrentExecutablePath() {
		wchar_t modulePath[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (length == 0) {
			return {};
		}
		return std::filesystem::path(modulePath);
	}

	bool LaunchSimulationExecutable() {
		const std::filesystem::path currentExe = GetCurrentExecutablePath();
		if (currentExe.empty()) {
			return false;
		}

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

	std::string GetJsonString(const json &objectData, const char *key) {
		if (!objectData.contains(key) || !objectData[key].is_string()) {
			return {};
		}
		return objectData[key].get<std::string>();
	}

	bool IsSceneCategory(const json &objectData, const char *category) {
		return GetJsonString(objectData, "category") == category;
	}

	bool IsSceneEnemyObject(const json &objectData) {
		return IsSceneCategory(objectData, "ENEMY") || objectData.contains("enemy");
	}

	bool IsScenePlayerObject(const json &objectData) {
		return IsSceneCategory(objectData, "PLAYER");
	}

	bool IsSceneObstacleObject(const json &objectData) {
		if (IsScenePlayerObject(objectData) || IsSceneEnemyObject(objectData)) {
			return false;
		}
		if (IsSceneCategory(objectData, "OBSTACLE")) {
			return true;
		}
		if (GetJsonString(objectData, "type") == "MESH") {
			std::string name = GetJsonString(objectData, "name");
			if (name.find("Terrain") != std::string::npos || name.find("terrain") != std::string::npos) {
				return true;
			}
		}
		return false;
	}

	json ToBlenderPositionJson(const Vector3 &position) {
		return json::array({ position.x, position.z, position.y });
	}

	json ToBlenderRotationJson(const Vector3 &rotation) {
		return json::array({
			rotation.x * kRadiansToDegrees,
			rotation.z * kRadiansToDegrees,
			rotation.y * kRadiansToDegrees
		});
	}

	json ToBlenderScaleJson(const Vector3 &scale) {
		return json::array({ scale.x, scale.z, scale.y });
	}

	void WriteSceneTransform(
		json &objectData,
		const Vector3 &position,
		const Vector3 *rotation,
		const Vector3 *scale) {

		if (!objectData.contains("transform") || !objectData["transform"].is_object()) {
			objectData["transform"] = json::object();
		}

		json &transform = objectData["transform"];
		transform["translation"] = ToBlenderPositionJson(position);
		if (rotation) {
			transform["rotation"] = ToBlenderRotationJson(*rotation);
		}
		if (scale) {
			transform["scale"] = ToBlenderScaleJson(*scale);
		}
	}

	Vector3 EulerFromQuaternionXYZ(const Quaternion &quaternion) {
		const Matrix4x4 matrix = MyMath::MakeRotateMatrix(MyMath::Normalize(quaternion));
		const float sinY = std::clamp(-matrix.m[0][2], -1.0f, 1.0f);
		const float y = std::asin(sinY);
		const float cosY = std::cos(y);

		Vector3 rotation = { 0.0f, y, 0.0f };
		if (std::abs(cosY) > 0.0001f) {
			rotation.x = std::atan2(matrix.m[1][2], matrix.m[2][2]);
			rotation.z = std::atan2(matrix.m[0][1], matrix.m[0][0]);
		} else {
			rotation.z = std::atan2(-matrix.m[1][0], matrix.m[1][1]);
		}
		return rotation;
	}

	bool SceneObjectNameExists(const json &objects, const std::string &name) {
		for (const auto &objectData : objects) {
			if (GetJsonString(objectData, "name") == name) {
				return true;
			}
		}
		return false;
	}

	std::string MakeUniqueSceneObjectName(const json &objects, const std::string &prefix) {
		for (int index = 1; index < 10000; ++index) {
			std::string name = prefix + std::to_string(index);
			if (!SceneObjectNameExists(objects, name)) {
				return name;
			}
		}
		return prefix + "9999";
	}

	std::string TrimActionName(const std::string &name) {
		size_t begin = 0;
		while (begin < name.size() && std::isspace(static_cast<unsigned char>(name[begin]))) {
			++begin;
		}

		size_t end = name.size();
		while (end > begin && std::isspace(static_cast<unsigned char>(name[end - 1]))) {
			--end;
		}

		return name.substr(begin, end - begin);
	}

	json ToVector3Json(const Vector3 &value) {
		return json::array({ value.x, value.y, value.z });
	}

	Vector3 ReadVector3Json(const json &value, const Vector3 &fallback) {
		if (!value.is_array() || value.size() < 3) {
			return fallback;
		}

		return {
			value[0].get<float>(),
			value[1].get<float>(),
			value[2].get<float>()
		};
	}

	float ReadJsonFloat(const json &objectData, const char *key, float fallback) {
		if (!objectData.is_object() || !objectData.contains(key) || !objectData[key].is_number()) {
			return fallback;
		}
		return objectData[key].get<float>();
	}

	int ReadJsonInt(const json &objectData, const char *key, int fallback) {
		if (!objectData.is_object() || !objectData.contains(key) || !objectData[key].is_number_integer()) {
			return fallback;
		}
		return objectData[key].get<int>();
	}

	json PlayerModeParamsToJson(const PlayerModeParams &params) {
		json data;
		data["maxMoveSpeed"] = params.maxMoveSpeed;
		data["moveAcceleration"] = params.moveAcceleration;
		data["moveDamping"] = params.moveDamping;
		data["pitchSpeed"] = params.pitchSpeed;
		data["yawSpeed"] = params.yawSpeed;
		data["rollSpeed"] = params.rollSpeed;
		return data;
	}

	void ApplyPlayerModeParamsFromJson(const json &data, PlayerModeParams &params) {
		params.maxMoveSpeed = ReadJsonFloat(data, "maxMoveSpeed", params.maxMoveSpeed);
		params.moveAcceleration = ReadJsonFloat(data, "moveAcceleration", params.moveAcceleration);
		params.moveDamping = ReadJsonFloat(data, "moveDamping", params.moveDamping);
		params.pitchSpeed = ReadJsonFloat(data, "pitchSpeed", params.pitchSpeed);
		params.yawSpeed = ReadJsonFloat(data, "yawSpeed", params.yawSpeed);
		params.rollSpeed = ReadJsonFloat(data, "rollSpeed", params.rollSpeed);
	}

	float LengthSqVector3(const Vector3 &value) {
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}

	float LengthVector3(const Vector3 &value) {
		return std::sqrt(LengthSqVector3(value));
	}

	Vector3 NormalizeOrVector3(const Vector3 &value, const Vector3 &fallback) {
		const float length = LengthVector3(value);
		if (length <= 0.0001f) {
			return fallback;
		}
		return ScaleVector3(value, 1.0f / length);
	}

	Vector3 LerpVector3(const Vector3 &from, const Vector3 &to, float t) {
		return {
			from.x + (to.x - from.x) * t,
			from.y + (to.y - from.y) * t,
			from.z + (to.z - from.z) * t
		};
	}

	Quaternion MakeLookQuaternion(const Vector3 &forward) {
		const Vector3 normalizedForward = NormalizeOrVector3(forward, { 0.0f, 0.0f, 1.0f });
		const float clampedY = std::clamp(normalizedForward.y, -1.0f, 1.0f);
		const float pitch = -std::asin(clampedY);
		const float yaw = std::atan2(normalizedForward.x, normalizedForward.z);

		Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
		return MyMath::Normalize(MyMath::Multiply(qYaw, qPitch));
	}

	bool IsImGuiKeyboardCaptureActive() {
#ifdef ENABLE_IMGUI
		if (!ImGuiManager::IsVisible() || ImGui::GetCurrentContext() == nullptr) {
			return false;
		}

		const ImGuiIO &io = ImGui::GetIO();
		return io.WantCaptureKeyboard || io.WantTextInput || ImGui::IsAnyItemActive();
#else
		return false;
#endif
	}

	bool IsImGuiMouseCaptureActive() {
#ifdef ENABLE_IMGUI
		if (!ImGuiManager::IsVisible() || ImGui::GetCurrentContext() == nullptr) {
			return false;
		}

		const ImGuiIO &io = ImGui::GetIO();
		return io.WantCaptureMouse || ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();
#else
		return false;
#endif
	}

	bool GetOverlayBounds(float &minX, float &minY, float &maxX, float &maxY) {
		if (FlyCamera::GetGameViewBounds(minX, minY, maxX, maxY) && maxX > minX && maxY > minY) {
			return true;
		}

		minX = 0.0f;
		minY = 0.0f;
		maxX = static_cast<float>(WinApp::GetClientWidth());
		maxY = static_cast<float>(WinApp::GetClientHeight());
		return maxX > minX && maxY > minY;
	}

	void DrawLockOnOverlaySprite(const Enemy *target, const Matrix4x4 &viewProjectionMatrix, Sprite* lockOnReticleSprite_) {
		if (!target) {
			return;
		}
		
		try {
			if (target->IsDead()) {
				return;
			}
		} catch(...) {
			return;
		}

		Vector3 worldPosition = target->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = target->GetCollisionRadius();
		} catch(...) {}
		worldPosition.y += collisionRadius * 0.3f;
		float minX = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float maxY = 0.0f;
		if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
			return;
		}

		const float width = maxX - minX;
		const float height = maxY - minY;

		Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
		if (screenPosition.z < 0.0f || screenPosition.z > 1.0f) {
			return;
		}
		if (screenPosition.x < 0.0f || screenPosition.x > width ||
			screenPosition.y < 0.0f || screenPosition.y > height) {
			return;
		}

		const Vector2 center = { minX + screenPosition.x, minY + screenPosition.y };
		const float reticleSize = std::clamp(76.0f + collisionRadius * 12.0f, 76.0f, 116.0f);

		if (lockOnReticleSprite_) {
			lockOnReticleSprite_->SetPosition(center);
			lockOnReticleSprite_->SetSize({ reticleSize, reticleSize });
			lockOnReticleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			lockOnReticleSprite_->Update();
			lockOnReticleSprite_->Draw();
		}
	}

	void DrawAimCursorOverlaySprite(Sprite* aimCursorSprite_) {
		float minX = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float maxY = 0.0f;
		if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
			return;
		}

		const Vector2 center = { (minX + maxX) * 0.5f, (minY + maxY) * 0.5f };
		if (aimCursorSprite_) {
			aimCursorSprite_->SetPosition(center);
			aimCursorSprite_->SetSize({ 44.0f, 44.0f });
			aimCursorSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 220.0f / 255.0f });
			aimCursorSprite_->Update();
			aimCursorSprite_->Draw();
		}
	}
}

#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
	bool gShowStageValidationWindow = true;
	bool gShowStageValidationLabels = true;

	ImVec4 ValidationLevelColor(const std::string &level) {
		if (level == "ERROR") {
			return ImVec4(1.0f, 0.25f, 0.2f, 1.0f);
		}
		if (level == "WARNING") {
			return ImVec4(1.0f, 0.75f, 0.2f, 1.0f);
		}
		return ImVec4(0.35f, 1.0f, 0.45f, 1.0f);
	}

	const char *ValidationLevelText(const std::string &level) {
		if (level == "ERROR") {
			return "ERROR";
		}
		if (level == "WARNING") {
			return "WARNING";
		}
		return "OK";
	}

	void DrawValidationMessages(const char *title, const std::vector<std::string> &messages, const ImVec4 &color) {
		ImGui::TextColored(color, "%s: %d", title, static_cast<int>(messages.size()));
		for (const std::string &message : messages) {
			ImGui::BulletText("%s", message.c_str());
		}
	}

	void DrawStageValidationWindow(const StageValidation::Report &report) {
		ImGui::Begin("Level Validation");
		ImGui::Text("Source: %s", report.source.c_str());

		DrawValidationMessages("Errors", report.errors, ValidationLevelColor("ERROR"));
		DrawValidationMessages("Warnings", report.warnings, ValidationLevelColor("WARNING"));

		ImGui::Separator();
		ImGui::Text("Check Items");
		for (const StageValidation::CheckItemResult &item : report.checkItems) {
			ImGui::TextColored(ValidationLevelColor(item.level), "%s", ValidationLevelText(item.level));
			ImGui::SameLine(90.0f);
			if (item.detail.empty()) {
				ImGui::TextWrapped("%s", item.label.c_str());
			} else {
				ImGui::TextWrapped("%s: %s", item.label.c_str(), item.detail.c_str());
			}
		}

		ImGui::End();
	}

	struct ValidationLabelRect {
		ImVec2 min;
		ImVec2 max;
	};

	float ClampValidationFloat(float value, float minValue, float maxValue) {
		if (maxValue < minValue) {
			return minValue;
		}
		if (value < minValue) {
			return minValue;
		}
		if (value > maxValue) {
			return maxValue;
		}
		return value;
	}

	ValidationLabelRect MakeValidationLabelRect(const ImVec2 &textPos, const ImVec2 &textSize, float padding) {
		return {
			ImVec2(textPos.x - padding, textPos.y - padding),
			ImVec2(textPos.x + textSize.x + padding, textPos.y + textSize.y + padding),
		};
	}

	bool ValidationRectsOverlap(const ValidationLabelRect &lhs, const ValidationLabelRect &rhs) {
		return lhs.min.x < rhs.max.x && lhs.max.x > rhs.min.x &&
			lhs.min.y < rhs.max.y && lhs.max.y > rhs.min.y;
	}

	bool ValidationRectOverlapsAny(const ValidationLabelRect &rect, const std::vector<ValidationLabelRect> &usedRects) {
		for (const ValidationLabelRect &usedRect : usedRects) {
			if (ValidationRectsOverlap(rect, usedRect)) {
				return true;
			}
		}
		return false;
	}

	void ClampValidationLabelToGameView(
		ImVec2 &textPos,
		const ImVec2 &textSize,
		float padding,
		float minX,
		float minY,
		float maxX,
		float maxY) {

		textPos.x = ClampValidationFloat(textPos.x, minX + padding, maxX - textSize.x - padding);
		textPos.y = ClampValidationFloat(textPos.y, minY + padding, maxY - textSize.y - padding);
	}

	ImVec2 ResolveValidationLabelPosition(
		const ImVec2 &anchor,
		const ImVec2 &textSize,
		float padding,
		float minX,
		float minY,
		float maxX,
		float maxY,
		std::vector<ValidationLabelRect> &usedRects) {

		const float rectWidth = textSize.x + padding * 2.0f;
		const float rectHeight = textSize.y + padding * 2.0f;
		const float verticalStep = rectHeight + 4.0f;
		const float horizontalStep = rectWidth * 0.55f;
		const int horizontalSlots[] = { 0, -1, 1, -2, 2 };

		for (int stackIndex = 0; stackIndex < 10; ++stackIndex) {
			for (int side = 0; side < 2; ++side) {
				for (int horizontalSlot : horizontalSlots) {
					const float x = anchor.x - textSize.x * 0.5f + horizontalStep * static_cast<float>(horizontalSlot);
					const float y = (side == 0)
						? anchor.y - textSize.y - 22.0f - verticalStep * static_cast<float>(stackIndex)
						: anchor.y + 16.0f + verticalStep * static_cast<float>(stackIndex);
					ImVec2 candidate(x, y);
					ClampValidationLabelToGameView(candidate, textSize, padding, minX, minY, maxX, maxY);

					const ValidationLabelRect rect = MakeValidationLabelRect(candidate, textSize, padding);
					if (!ValidationRectOverlapsAny(rect, usedRects)) {
						usedRects.push_back(rect);
						return candidate;
					}
				}
			}
		}

		ImVec2 fallback(anchor.x - textSize.x * 0.5f, anchor.y - textSize.y - 22.0f);
		ClampValidationLabelToGameView(fallback, textSize, padding, minX, minY, maxX, maxY);
		usedRects.push_back(MakeValidationLabelRect(fallback, textSize, padding));
		return fallback;
	}

	void DrawStageValidationOverlay(const StageValidation::Report &report, const Matrix4x4 &viewProjectionMatrix) {
		if (!report.HasMarkers()) {
			return;
		}

		float minX = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float maxY = 0.0f;
		if (!FlyCamera::GetGameViewBounds(minX, minY, maxX, maxY)) {
			return;
		}

		const float width = maxX - minX;
		const float height = maxY - minY;
		if (!ImGui::GetCurrentContext()) {
			return;
		}
		ImDrawList *drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
		std::vector<ValidationLabelRect> usedRects;

		for (const StageValidation::ValidationMarker &marker : report.markers) {
			Vector3 worldPosition = {
				static_cast<float>(marker.x),
				static_cast<float>(marker.y) + 1.2f,
				static_cast<float>(marker.z),
			};
			Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
			if (screenPosition.z < 0.0f || screenPosition.z > 1.0f) {
				continue;
			}
			if (screenPosition.x < 0.0f || screenPosition.x > width || screenPosition.y < 0.0f || screenPosition.y > height) {
				continue;
			}

			const bool isError = marker.level == "ERROR";
			const std::string text = std::string(isError ? "ERROR: " : "WARNING: ") + marker.message;
			const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
			const float padding = 4.0f;
			const ImVec2 anchor(minX + screenPosition.x, minY + screenPosition.y);
			const ImVec2 textPos = ResolveValidationLabelPosition(anchor, textSize, padding, minX, minY, maxX, maxY, usedRects);
			const ValidationLabelRect rect = MakeValidationLabelRect(textPos, textSize, padding);

			const ImU32 fillColor = isError ? IM_COL32(90, 16, 16, 220) : IM_COL32(92, 63, 0, 220);
			const ImU32 borderColor = isError ? IM_COL32(255, 75, 55, 255) : IM_COL32(255, 200, 45, 255);
			drawList->AddRectFilled(rect.min, rect.max, fillColor, 3.0f);
			drawList->AddRect(rect.min, rect.max, borderColor, 3.0f);
			drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text.c_str());
		}
	}
#endif

GamePlayScene::GamePlayScene(Mode mode)
	: mode_(mode) {
}


void GamePlayScene::Initialize() {

	//カメラ・シーンリソース
	camera = std::make_unique<Camera>();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	//スプライト�E初期匁E
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(SpriteCommon::GetInstance() , "resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture(kLockOnReticleTexturePath);
	TextureManager::GetInstance()->LoadTexture(kAimCursorTexturePath);

	aimCursorSprite_ = std::make_unique<Sprite>();
	aimCursorSprite_->Initialize(SpriteCommon::GetInstance(), kAimCursorTexturePath);
	aimCursorSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	lockOnReticleSprite_ = std::make_unique<Sprite>();
	lockOnReticleSprite_->Initialize(SpriteCommon::GetInstance(), kLockOnReticleTexturePath);
	lockOnReticleSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	// SkyboxCommon に DirectX の惁E��を渡して初期化する！E
	SkyboxCommon::GetInstance()->Initialize(DirectXCommon::GetInstance());

	// スカイボックスの生�Eと初期匁E
	skybox = std::make_unique<Skybox>();
	skybox->Initialize("resources/SkyBox.dds");


	//Model��・パ�EチE��クル
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("multiMesh.obj");
	ModelManager::GetInstance()->CreateSphereModel("Sphere", 16);

	//======================================================
	// プリミティブE生EEE
	//======================================================

	// 地面のモデル
	groundModel = std::make_unique<Object3d>();
	groundModel->Initialize(Object3dCommon::GetInstance());
	groundModel->SetModel("plane.obj");
	groundModel->SetScale({ 100.0f, 1.0f, 100.0f });
	groundModel->SetTranslate({ 0.0f, 0.0f, 0.0f });
	objects.push_back(groundModel.get());

	// 琁EE
	myShere = std::make_unique<Primitive>();
	myShere->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Sphere);
	myShere->SetTranslate({ 2.0f,0.0f,0.0f });
	// objects.push_back(myShere.get());
	// ボ�Eンとしても使われるため、目立つように赤くしておく
	if (myShere->GetModel()) {
		myShere->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
	}

	// ボックス
	myBox = std::make_unique<Primitive>();
	myBox->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Box);
	myBox->SetTranslate({ -2.0f,0.0f,0.0f });
	// objects.push_back(myBox.get()); // Boxの代わりにModel��を使ぁE

	// 動的Model��
	myModelObject = std::make_unique<Object3d>();
	myModelObject->Initialize(Object3dCommon::GetInstance());
	ModelManager::GetInstance()->LoadModel("AnimatedCube/AnimatedCube.gltf");
	myModelObject->SetModel("AnimatedCube/AnimatedCube.gltf");
	//objects.push_back(myModelObject.get());

	// アニメーションとノ�Eド階層の読み込み
	animationData = LoadAnimationFile("resources/AnimatedCube", "AnimatedCube.gltf");
	Node rootNode = Model::LoadNodeHierarchy("resources/AnimatedCube", "AnimatedCube.gltf");
	skeleton = CreateSkeleton(rootNode);
	if (!skeleton.joints.empty()) {
		skeleton.joints[skeleton.root].transform.translate = { 0.0f, 0.0f, 0.0f };
	}

	myModelObject->skinCluster = myModelObject->GetModel()->CreateSkinCluster(skeleton);

	// ボ�Eンライン用オブジェクト�E初期匁E
	ModelManager::GetInstance()->CreateLineModel("SkeletonLines");
	skeletonLinesObject = std::make_unique<Object3d>();
	skeletonLinesObject->Initialize(Object3dCommon::GetInstance());
	skeletonLinesObject->SetModel("SkeletonLines");

	// チE��チE��用コライダー表示ラインオブジェクト�E初期匁E
	ModelManager::GetInstance()->CreateLineModel("DebugColliderLines");
	debugColliderLinesObject = std::make_unique<Object3d>();
	debugColliderLinesObject->Initialize(Object3dCommon::GetInstance());
	debugColliderLinesObject->SetModel("DebugColliderLines");

	// チE��チE��用フリーカメラの初期匁E
	debugFlyCamera_ = std::make_unique<FlyCamera>();
	debugFlyCamera_->SetTranslate({ 0.0f, 5.0f, -20.0f }); // 初期位置
	isDebugCameraActive_ = false;


	// リング
	myRing = std::make_unique<Primitive>();
	myRing->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Ring);
	myRing->SetTranslate({ 0.0f, 0.0f, 0.0f }); // パ�EチE��クルと同じ中忁E��置に合わせる

	// 部刁E��ング (三日朁E
	myPartialRing = std::make_unique<Primitive>();
	myPartialRing->Initialize(Object3dCommon::GetInstance(), PrimitiveType::PartialRing);
	myPartialRing->SetTranslate({ 0.0f, 0.0f, 0.0f });

	// 冁E��エフェクチE
	myCylinder = std::make_unique<Primitive>();
	myCylinder->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Cylinder);
	myCylinder->SetTranslate({ 0.0f, 0.0f, 0.0f });
	myCylinder->SetScale({ 2.0f, 2.0f, 2.0f });

	//パ�EチE��クル
	particleManager = std::make_unique<ParticleManager>();
	particleManager->Initialize(DirectXCommon::GetInstance());
	particleManager->CreateParticleGroup("test", "resources/circle.png");
	particleEmitter = std::make_unique<ParticleEmitter>("test", Vector3{ 0.0f,0.0f,0.0f }, particleManager.get());

	//音声再生
	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");

	pVoice1=AudioManager::GetInstance()->PlayWave(soundData1, true);
	pVoice2=AudioManager::GetInstance()->PlayWave(soundData2, true);

	// 1. マネージャー経由でトレイル専用Model��を作る
	ModelManager::GetInstance()->CreateTrailModel("SmokeTrail");

	// 2. トレイル計算機�E初期化（今回は60フレーム=紁E秒�Eの長さを残す�E�E
	missileTrail = std::make_unique<Trail>();
	missileTrail->Initialize(60);

	// 3. 描画用オブジェクト�E初期匁E
	trailObject = std::make_unique<Object3d>();
	trailObject->Initialize(Object3dCommon::GetInstance());
	trailObject->SetModel("SmokeTrail");


	ModelManager::GetInstance()->CreateBoxModel("PlayerBox");
	ModelManager::GetInstance()->CreateBoxModel("EnemyBox");
	ModelManager::GetInstance()->CreateBoxModel("ObstacleBox");

	player_ = std::make_unique<Player>();
	player_->Initialize(kPlayerModelName);

	// 弾
	missileManager_ = std::make_unique<MissileManager>();
	missileManager_->Initialize();

	// 爁E��エフェクチE
	explosionManager_ = std::make_unique<ExplosionManager>();
	explosionManager_->Initialize(particleManager.get());

	//敵
	enemies_.clear();
	auto firstEnemy = std::make_unique<Enemy>();
	firstEnemy->Initialize({ 0.0f, 0.0f, 50.0f });
	enemies_.push_back(std::move(firstEnemy));

	enemyBulletManager_ = std::make_unique<EnemyBulletManager>();
	enemyBulletManager_->Initialize();

	// ゲームオーバ�E演�Eの初期匁E
	isGameOver_ = false;
	gameOverTimer_ = 0;

	ReloadSceneJson();
// 	RefreshSimulationActionNames();
// 	RefreshMissilePresetNames();

	if (IsSimulationMode()) {
		isEditorPreviewPlaying_ = false;
		currentSimulationTarget_ = 2;
		showSimulationWindow_ = true;
		SetDebugCameraActive(true);
	}

	// エチE��ターレシーバ�Eの初期匁E
	EditorReceiver::GetInstance()->Initialize();
}

void GamePlayScene::SetDebugCameraActive(bool isActive) {
	if (isDebugCameraActive_ == isActive) {
		return;
	}

	isDebugCameraActive_ = isActive;
	isCinematicLockOnCameraInitialized_ = false;
	if (isDebugCameraActive_) {
		debugFlyCamera_->SetTranslate(camera->GetTranslate());
		Object3dCommon::GetInstance()->SetDefaultCamera(debugFlyCamera_.get());
		OutputDebugStringA("[DebugCamera] ON: FlyCamera\n");
	} else {
		Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
		OutputDebugStringA("[DebugCamera] OFF: Player Camera\n");
	}
}

void GamePlayScene::ReloadSceneJson() {
	lockedEnemy_ = nullptr;
	aimAssistEnemy_ = nullptr;
	CancelMultiLock();
	isCinematicLockOnCameraInitialized_ = false;
	enemies_.clear();
	obstacles_.clear();
	enemySpawns_.clear();

	StageLoader::LoadSceneJson("resources/scene.json", enemies_, obstacles_, player_.get(), &enemySpawns_);

	// イベントデータを�E読み込み
	enemyEventManager_.LoadEvents("resources/enemy_events.json");
	for (auto& spawnData : enemySpawns_) {
		if (spawnData.HasReinforcementTrigger() || enemyEventManager_.IsTargetEnemy(spawnData.name)) {
			spawnData.isInitialSpawn = false;
		}
	}

// 	SpawnEnemiesFromSpawnPoints();

	try {
		lastJsonWriteTime_ = std::filesystem::last_write_time("resources/scene.json");
	} catch (...) {
		// JSONがまだ存在しなぁE��合でも、エチE��タ操作を続けられるよぁE��する"
	}
}

void GamePlayScene::ResetEditorPreview() {
	isEditorPreviewPlaying_ = false;
	isGameOver_ = false;
	gameOverTimer_ = 0;
	lockedEnemy_ = nullptr;
	aimAssistEnemy_ = nullptr;
	CancelMultiLock();
	isCinematicLockOnCameraInitialized_ = false;

	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	if (player_) {
		player_->Initialize(kPlayerModelName);
	}
	if (missileManager_) {
		missileManager_->Initialize();
	}
	if (enemyBulletManager_) {
		enemyBulletManager_->Initialize();
	}
	if (explosionManager_) {
		explosionManager_->Initialize(particleManager.get());
	}

	ReloadSceneJson();

	if (!isDebugCameraActive_ && player_) {
		Vector3* targetPos = nullptr;
		Vector3 enemyPos;
		if (lockedEnemy_) {
			enemyPos = lockedEnemy_->GetPosition();
			targetPos = &enemyPos;
		}
		player_->UpdateCamera(camera.get(), targetPos);
	}

	OutputDebugStringA("[EditorPreview] Reset scene and paused.\n");
}

bool GamePlayScene::SaveCurrentSimulationLayoutToSceneJson(const std::string &filePath) {
	json root;
	{
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) {
			simulationSaveMessage_ = "scene.json が見つからなぁE��め保存できませんでした";
			OutputDebugStringA(("[SimulationSave] File not found: " + filePath + "\n").c_str());
			return false;
		}

		try {
			ifs >> root;
		} catch (const std::exception &e) {
			simulationSaveMessage_ = "scene.json の読み込みに失敗しました";
			OutputDebugStringA(("[SimulationSave] JSON parse failed: " + std::string(e.what()) + "\n").c_str());
			return false;
		}
	}

	if (!root.contains("objects") || !root["objects"].is_array()) {
		simulationSaveMessage_ = "scene.json に objects がなぁE��め保存できませんでした";
		OutputDebugStringA("[SimulationSave] objects array not found.\n");
		return false;
	}

	json &objects = root["objects"];
	size_t playerObjectIndex = kInvalidSceneObjectIndex;
	std::vector<size_t> enemyObjectIndices;
	std::vector<size_t> obstacleObjectIndices;

	for (size_t index = 0; index < objects.size(); ++index) {
		const json &objectData = objects[index];
		if (IsScenePlayerObject(objectData) && playerObjectIndex == kInvalidSceneObjectIndex) {
			playerObjectIndex = index;
		}
		if (IsSceneEnemyObject(objectData)) {
			enemyObjectIndices.push_back(index);
		} else if (IsSceneObstacleObject(objectData)) {
			obstacleObjectIndices.push_back(index);
		}
	}

	size_t savedCount = 0;

	if (player_ && playerObjectIndex != kInvalidSceneObjectIndex) {
		const Vector3 playerRotation = EulerFromQuaternionXYZ(player_->GetQuaternion());
		WriteSceneTransform(objects[playerObjectIndex], player_->GetPosition(), &playerRotation, nullptr);
		++savedCount;
	}

	size_t obstacleIndex = 0;
	for (const auto &obstacle : obstacles_) {
		if (!obstacle || obstacleIndex >= obstacleObjectIndices.size()) {
			break;
		}

		const Vector3 obstacleRotation = obstacle->GetRotation();
		const Vector3 obstacleScale = obstacle->GetScale();
		WriteSceneTransform(
			objects[obstacleObjectIndices[obstacleIndex]],
			obstacle->GetPosition(),
			&obstacleRotation,
			&obstacleScale);
		++obstacleIndex;
		++savedCount;
	}

	for (const auto &enemy : enemies_) {
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		const Vector3 enemyPosition = enemy->GetPosition();
		const Vector3 enemyRotation = enemy->GetRotation();
		const size_t spawnPointIndex = enemy->GetSpawnPointIndex();

		if (spawnPointIndex != Enemy::kNoSpawnPoint && spawnPointIndex < enemyObjectIndices.size()) {
			WriteSceneTransform(objects[enemyObjectIndices[spawnPointIndex]], enemyPosition, &enemyRotation, nullptr);
			if (spawnPointIndex < enemySpawns_.size()) {
				enemySpawns_[spawnPointIndex].position = enemyPosition;
				enemySpawns_[spawnPointIndex].rotation = enemyRotation;
			}
			++savedCount;
			continue;
		}

		json newEnemy;
		newEnemy["type"] = "MESH";
		newEnemy["name"] = MakeUniqueSceneObjectName(objects, "SimEnemy");
		newEnemy["category"] = "ENEMY";
		newEnemy["enemy"] = { { "type", "VF1" } };
		newEnemy["vertices_count"] = 0;
		const Vector3 enemyScale = enemy->GetScale();
		WriteSceneTransform(newEnemy, enemyPosition, &enemyRotation, &enemyScale);

		objects.push_back(newEnemy);

		EnemySpawnData spawnData;
		spawnData.name = newEnemy["name"].get<std::string>();
		spawnData.position = enemyPosition;
		spawnData.rotation = enemyRotation;
		spawnData.isInitialSpawn = true;
		enemySpawns_.push_back(spawnData);
		enemy->SetSpawnPointIndex(enemySpawns_.size() - 1);
		enemyObjectIndices.push_back(objects.size() - 1);
		++savedCount;
	}

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		simulationSaveMessage_ = "scene.json を書き込めませんでした";
		OutputDebugStringA(("[SimulationSave] Failed to open for write: " + filePath + "\n").c_str());
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	try {
		lastJsonWriteTime_ = std::filesystem::last_write_time(filePath);
	} catch (...) {
	}

	simulationSaveMessage_ = "現在の配置めEscene.json に保存しました。実ゲームにも反映されます";
	OutputDebugStringA(("[SimulationSave] Saved " + std::to_string(savedCount) + " transforms.\n").c_str());
	return true;
}










void GamePlayScene::RefreshSimulationActionNames() {
	simulationActionNames_.clear();

	std::ifstream ifs(kSimulationActionsFilePath);
	if (!ifs.is_open()) {
		selectedSimulationActionIndex_ = 0;
		return;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		selectedSimulationActionIndex_ = 0;
		return;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		selectedSimulationActionIndex_ = 0;
		return;
	}

	for (const auto &actionData : root["actions"]) {
		if (!actionData.is_object() || !actionData.contains("name") || !actionData["name"].is_string()) {
			continue;
		}

		simulationActionNames_.push_back(actionData["name"].get<std::string>());
	}

	if (selectedSimulationActionIndex_ >= static_cast<int>(simulationActionNames_.size())) {
		selectedSimulationActionIndex_ = simulationActionNames_.empty() ? 0 : static_cast<int>(simulationActionNames_.size()) - 1;
	}
}

bool GamePlayScene::SaveNamedSimulationAction(const std::string &filePath, const std::string &actionName) {
	const std::string trimmedName = TrimActionName(actionName);
	if (trimmedName.empty()) {
		simulationActionMessage_ = "保存名を入力してください。";
		return false;
	}

	json root;
	{
		std::ifstream ifs(filePath);
		if (ifs.is_open()) {
			try {
				ifs >> root;
			} catch (...) {
				root = json::object();
			}
		}
	}

	if (!root.is_object()) {
		root = json::object();
	}
	if (!root.contains("actions") || !root["actions"].is_array()) {
		root["actions"] = json::array();
	}

	json action;
	action["name"] = trimmedName;

	if (player_) {
		json playerData;
		playerData["position"] = ToVector3Json(player_->GetPosition());
		playerData["rotation"] = ToVector3Json(EulerFromQuaternionXYZ(player_->GetQuaternion()));
		playerData["mode"] = static_cast<int>(player_->GetCurrentMode());

		json modeParams = json::array();
		for (int modeIndex = 0; modeIndex < 3; ++modeIndex) {
			modeParams.push_back(PlayerModeParamsToJson(player_->GetModeParams(static_cast<PlayerMode>(modeIndex))));
		}
		playerData["modeParams"] = modeParams;
		action["player"] = playerData;
	}

	json missileData;
	missileData["normalSpeed"] = missileNormalSpeed;
	missileData["normalScale"] = missileNormalScale;
	missileData["normalCollisionRadius"] = missileNormalCollisionRadius;
	missileData["normalLifeTime"] = missileNormalLifeTime;
	missileData["speed"] = missileSpeed;
	missileData["ampX"] = missileAmpX;
	missileData["ampZ"] = missileAmpZ;
	missileData["ampY"] = missileAmpY;
	missileData["freqY"] = missileFreqY;
	missileData["baseY"] = missileBaseY;
	missileData["homingStrength"] = missileHomingStrength;
	missileData["homingScale"] = missileHomingScale;
	missileData["homingCollisionRadius"] = missileHomingCollisionRadius;
	missileData["trailWidth"] = missileTrailWidth;
	missileData["lifeTime"] = missileLifeTime;
	missileData["muzzleOffset"] = missileMuzzleOffset;
	action["missile"] = missileData;

	json enemiesData = json::array();
	int generatedEnemyIndex = 1;
	for (const auto &enemy : enemies_) {
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		json enemyData;
		const size_t spawnPointIndex = enemy->GetSpawnPointIndex();
		if (spawnPointIndex < enemySpawns_.size()) {
			const EnemySpawnData &spawnData = enemySpawns_[spawnPointIndex];
			enemyData["name"] = spawnData.name;
			enemyData["reinforcementTrigger"] = spawnData.reinforcementTriggerName;
			enemyData["reinforcementDelay"] = spawnData.reinforcementDelayFrames;
		} else {
			enemyData["name"] = "SimEnemy" + std::to_string(generatedEnemyIndex++);
		}

		enemyData["position"] = ToVector3Json(enemy->GetPosition());
		enemyData["rotation"] = ToVector3Json(enemy->GetRotation());
		enemyData["scale"] = ToVector3Json(enemy->GetScale());
		enemyData["initial"] = true;
		enemiesData.push_back(enemyData);
	}
	action["enemies"] = enemiesData;

	json obstaclesData = json::array();
	for (const auto &obstacle : obstacles_) {
		if (!obstacle) {
			continue;
		}

		json obstacleData;
		obstacleData["position"] = ToVector3Json(obstacle->GetPosition());
		obstacleData["rotation"] = ToVector3Json(obstacle->GetRotation());
		obstacleData["scale"] = ToVector3Json(obstacle->GetScale());
		obstacleData["collisionOffset"] = ToVector3Json(obstacle->GetCollisionOffset());
		obstacleData["collisionScale"] = ToVector3Json(obstacle->GetCollisionScale());
		obstacleData["isCollisionEnabled"] = obstacle->IsCollisionEnabled();
		obstacleData["useMeshCollider"] = obstacle->IsUseMeshCollider();
		obstacleData["stageBounds"] = obstacle->IsStageBounds();
		obstaclesData.push_back(obstacleData);
	}
	action["obstacles"] = obstaclesData;

	if (explosionManager_) {
		const auto &config = explosionManager_->GetConfig();
		json explosionData;
		explosionData["count"] = config.count;
		explosionData["color"] = json::array({ config.color[0], config.color[1], config.color[2], config.color[3] });
		explosionData["speed"] = config.speed;
		explosionData["speedVariance"] = config.speedVariance;
		explosionData["scale"] = config.scale;
		explosionData["scaleVariance"] = config.scaleVariance;
		explosionData["lifeTimeMin"] = config.lifeTimeMin;
		explosionData["lifeTimeMax"] = config.lifeTimeMax;
		explosionData["posVariance"] = config.posVariance;
		action["explosion"] = explosionData;
	}

	json &actions = root["actions"];
	bool updated = false;
	for (auto &existingAction : actions) {
		if (existingAction.is_object() && existingAction.value("name", "") == trimmedName) {
			existingAction = action;
			updated = true;
			break;
		}
	}
	if (!updated) {
		actions.push_back(action);
	}

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		simulationActionMessage_ = "名前付き行動を保存できませんでした。";
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	RefreshSimulationActionNames();
	for (size_t index = 0; index < simulationActionNames_.size(); ++index) {
		if (simulationActionNames_[index] == trimmedName) {
			selectedSimulationActionIndex_ = static_cast<int>(index);
			break;
		}
	}

	simulationActionMessage_ = "行動「" + trimmedName + "」を保存しました。";
	return true;
}

bool GamePlayScene::ApplySimulationAction(const std::string &filePath, const std::string &actionName) {
	const std::string trimmedName = TrimActionName(actionName);
	if (trimmedName.empty()) {
		simulationActionMessage_ = "読み込む行動名がありません。";
		return false;
	}

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		simulationActionMessage_ = "名前付き行動ファイルが見つかりません。";
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		simulationActionMessage_ = "名前付き行動ファイルを読み込めませんでした。";
		return false;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		simulationActionMessage_ = "保存された行動がありません。";
		return false;
	}

	const json *action = nullptr;
	for (const auto &actionData : root["actions"]) {
		if (actionData.is_object() && actionData.value("name", "") == trimmedName) {
			action = &actionData;
			break;
		}
	}

	if (!action) {
		simulationActionMessage_ = "選択した行動が見つかりません。";
		return false;
	}

	lockedEnemy_ = nullptr;
	aimAssistEnemy_ = nullptr;
	CancelMultiLock();
	isCinematicLockOnCameraInitialized_ = false;
	isGameOver_ = false;
	gameOverTimer_ = 0;

	if (player_ && action->contains("player") && (*action)["player"].is_object()) {
		const json &playerData = (*action)["player"];
		if (playerData.contains("modeParams") && playerData["modeParams"].is_array()) {
			const json &modeParams = playerData["modeParams"];
			for (int modeIndex = 0; modeIndex < 3 && modeIndex < static_cast<int>(modeParams.size()); ++modeIndex) {
				ApplyPlayerModeParamsFromJson(modeParams[modeIndex], player_->GetModeParams(static_cast<PlayerMode>(modeIndex)));
			}
		}

		const int modeIndex = std::clamp(ReadJsonInt(playerData, "mode", 0), 0, 2);
		player_->ChangeMode(static_cast<PlayerMode>(modeIndex));
		player_->SetPosition(ReadVector3Json(playerData.value("position", json::array()), player_->GetPosition()));
		player_->SetRotation(ReadVector3Json(playerData.value("rotation", json::array()), { 0.0f, 0.0f, 0.0f }));
	}

	if (action->contains("missile") && (*action)["missile"].is_object()) {
		const json &missileData = (*action)["missile"];
		missileNormalSpeed = ReadJsonFloat(missileData, "normalSpeed", missileNormalSpeed);
		missileNormalScale = ReadJsonFloat(missileData, "normalScale", missileNormalScale);
		missileNormalCollisionRadius = ReadJsonFloat(missileData, "normalCollisionRadius", missileNormalCollisionRadius);
		missileNormalLifeTime = ReadJsonInt(missileData, "normalLifeTime", missileNormalLifeTime);
		missileSpeed = ReadJsonFloat(missileData, "speed", missileSpeed);
		missileAmpX = ReadJsonFloat(missileData, "ampX", missileAmpX);
		missileAmpZ = ReadJsonFloat(missileData, "ampZ", missileAmpZ);
		missileAmpY = ReadJsonFloat(missileData, "ampY", missileAmpY);
		missileFreqY = ReadJsonFloat(missileData, "freqY", missileFreqY);
		missileBaseY = ReadJsonFloat(missileData, "baseY", missileBaseY);
		missileHomingStrength = ReadJsonFloat(missileData, "homingStrength", missileHomingStrength);
		missileHomingScale = ReadJsonFloat(missileData, "homingScale", missileHomingScale);
		missileHomingCollisionRadius = ReadJsonFloat(missileData, "homingCollisionRadius", missileHomingCollisionRadius);
		missileTrailWidth = ReadJsonFloat(missileData, "trailWidth", missileTrailWidth);
		missileLifeTime = ReadJsonInt(missileData, "lifeTime", missileLifeTime);
		missileMuzzleOffset = ReadJsonFloat(missileData, "muzzleOffset", missileMuzzleOffset);
	}

	if (explosionManager_ && action->contains("explosion") && (*action)["explosion"].is_object()) {
		const json &explosionData = (*action)["explosion"];
		auto &config = explosionManager_->GetConfig();
		config.count = ReadJsonInt(explosionData, "count", config.count);
		if (explosionData.contains("color") && explosionData["color"].is_array() && explosionData["color"].size() >= 4) {
			for (int index = 0; index < 4; ++index) {
				config.color[index] = explosionData["color"][index].get<float>();
			}
		}
		config.speed = ReadJsonFloat(explosionData, "speed", config.speed);
		config.speedVariance = ReadJsonFloat(explosionData, "speedVariance", config.speedVariance);
		config.scale = ReadJsonFloat(explosionData, "scale", config.scale);
		config.scaleVariance = ReadJsonFloat(explosionData, "scaleVariance", config.scaleVariance);
		config.lifeTimeMin = ReadJsonFloat(explosionData, "lifeTimeMin", config.lifeTimeMin);
		config.lifeTimeMax = ReadJsonFloat(explosionData, "lifeTimeMax", config.lifeTimeMax);
		config.posVariance = ReadJsonFloat(explosionData, "posVariance", config.posVariance);
	}

	if (action->contains("obstacles") && (*action)["obstacles"].is_array()) {
		auto obstacleIt = obstacles_.begin();
		for (const auto &obstacleData : (*action)["obstacles"]) {
			if (obstacleIt == obstacles_.end()) {
				break;
			}
			if (!(*obstacleIt)) {
				++obstacleIt;
				continue;
			}

			(*obstacleIt)->SetPosition(ReadVector3Json(obstacleData.value("position", json::array()), (*obstacleIt)->GetPosition()));
			(*obstacleIt)->SetRotation(ReadVector3Json(obstacleData.value("rotation", json::array()), (*obstacleIt)->GetRotation()));
			(*obstacleIt)->SetScale(ReadVector3Json(obstacleData.value("scale", json::array()), (*obstacleIt)->GetScale()));
			(*obstacleIt)->SetCollisionOffset(ReadVector3Json(obstacleData.value("collisionOffset", json::array()), (*obstacleIt)->GetCollisionOffset()));
			(*obstacleIt)->SetCollisionScale(ReadVector3Json(obstacleData.value("collisionScale", json::array()), (*obstacleIt)->GetCollisionScale()));
			(*obstacleIt)->SetCollisionEnabled(obstacleData.value("isCollisionEnabled", (*obstacleIt)->IsCollisionEnabled()));
			(*obstacleIt)->SetUseMeshCollider(obstacleData.value("useMeshCollider", (*obstacleIt)->IsUseMeshCollider()));
			(*obstacleIt)->Update();
			++obstacleIt;
		}
	}

	if (action->contains("enemies") && (*action)["enemies"].is_array()) {
		enemies_.clear();
		enemySpawns_.clear();

		for (const auto &enemyData : (*action)["enemies"]) {
			if (!enemyData.is_object()) {
				continue;
			}

			EnemySpawnData spawnData;
			spawnData.name = enemyData.value("name", "SavedEnemy");
			spawnData.position = ReadVector3Json(enemyData.value("position", json::array()), { 0.0f, 0.0f, 0.0f });
			spawnData.rotation = ReadVector3Json(enemyData.value("rotation", json::array()), { 0.0f, 0.0f, 0.0f });
			spawnData.isInitialSpawn = enemyData.value("initial", true);
			spawnData.reinforcementTriggerName = enemyData.value("reinforcementTrigger", "");
			spawnData.reinforcementDelayFrames = ReadJsonInt(enemyData, "reinforcementDelay", 0);
			enemySpawns_.push_back(spawnData);

			if (spawnData.isInitialSpawn) {
				auto enemy = std::make_unique<Enemy>();
				enemy->Initialize(spawnData.position);
				enemy->SetRotation(spawnData.rotation);
				enemy->SetScale(ReadVector3Json(enemyData.value("scale", json::array()), enemy->GetScale()));
				enemy->SetSpawnPointIndex(enemySpawns_.size() - 1);
				enemies_.push_back(std::move(enemy));
			}
		}
		enemyRespawnTimers_.assign(enemySpawns_.size(), kNoRespawnTimer);
	}

	if (missileManager_) {
		missileManager_->Initialize();
	}
	if (enemyBulletManager_) {
		enemyBulletManager_->Initialize();
	}

	if (!isDebugCameraActive_ && camera && player_) {
		Vector3 *targetPos = nullptr;
		Vector3 enemyPos;
		if (lockedEnemy_) {
			enemyPos = lockedEnemy_->GetPosition();
			targetPos = &enemyPos;
		}
		player_->UpdateCamera(camera.get(), targetPos);
		camera->Update();
	} else if (debugFlyCamera_) {
		debugFlyCamera_->Camera::Update();
	}

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	if (player_) {
		player_->UpdateModel();
	}
	for (auto &enemy : enemies_) {
		enemy->UpdateModel();
	}
	for (auto &obstacle : obstacles_) {
		obstacle->Update();
	}
	if (missileManager_) {
		missileManager_->UpdateModels(activeCamera);
	}

	simulationActionMessage_ = "設定「" + trimmedName + "」をゲームに読み込みました。";
	return true;
}

void GamePlayScene::RefreshMissilePresetNames() {
	missilePresetNames_[0].clear();
	missilePresetNames_[1].clear();

	std::ifstream ifs(kMissilePresetsFilePath);
	if (!ifs.is_open()) {
		selectedMissilePresetIndex_[0] = 0;
		selectedMissilePresetIndex_[1] = 0;
		return;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		selectedMissilePresetIndex_[0] = 0;
		selectedMissilePresetIndex_[1] = 0;
		return;
	}

	const char *keys[] = { "normal", "homing" };
	for (int typeIndex = 0; typeIndex < 2; ++typeIndex) {
		if (!root.contains(keys[typeIndex]) || !root[keys[typeIndex]].is_array()) {
			continue;
		}

		for (const auto &presetData : root[keys[typeIndex]]) {
			if (presetData.is_object() && presetData.contains("name") && presetData["name"].is_string()) {
				missilePresetNames_[typeIndex].push_back(presetData["name"].get<std::string>());
			}
		}

		if (selectedMissilePresetIndex_[typeIndex] >= static_cast<int>(missilePresetNames_[typeIndex].size())) {
			selectedMissilePresetIndex_[typeIndex] = missilePresetNames_[typeIndex].empty()
				? 0
				: static_cast<int>(missilePresetNames_[typeIndex].size()) - 1;
		}
	}
}

bool GamePlayScene::SaveMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName) {
	const int typeIndex = std::clamp(missileTypeIndex, 0, 1);
	const std::string trimmedName = TrimActionName(presetName);
	if (trimmedName.empty()) {
		missilePresetMessage_ = "保存名を入力してください。";
		return false;
	}

	json root;
	{
		std::ifstream ifs(filePath);
		if (ifs.is_open()) {
			try {
				ifs >> root;
			} catch (...) {
				root = json::object();
			}
		}
	}
	if (!root.is_object()) {
		root = json::object();
	}

	const char *keys[] = { "normal", "homing" };
	if (!root.contains(keys[typeIndex]) || !root[keys[typeIndex]].is_array()) {
		root[keys[typeIndex]] = json::array();
	}

	json preset;
	preset["name"] = trimmedName;
	if (typeIndex == 0) {
		preset["speed"] = missileNormalSpeed;
		preset["scale"] = missileNormalScale;
		preset["collisionRadius"] = missileNormalCollisionRadius;
		preset["lifeTime"] = missileNormalLifeTime;
	} else {
		preset["speed"] = missileSpeed;
		preset["homingStrength"] = missileHomingStrength;
		preset["scale"] = missileHomingScale;
		preset["collisionRadius"] = missileHomingCollisionRadius;
		preset["trailWidth"] = missileTrailWidth;
		preset["lifeTime"] = missileLifeTime;
	}
	preset["muzzleOffset"] = missileMuzzleOffset;

	json &presets = root[keys[typeIndex]];
	bool updated = false;
	for (auto &existingPreset : presets) {
		if (existingPreset.is_object() && existingPreset.value("name", "") == trimmedName) {
			existingPreset = preset;
			updated = true;
			break;
		}
	}
	if (!updated) {
		presets.push_back(preset);
	}

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		missilePresetMessage_ = "ミサイル設定を保存できませんでした。";
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	RefreshMissilePresetNames();
	for (size_t index = 0; index < missilePresetNames_[typeIndex].size(); ++index) {
		if (missilePresetNames_[typeIndex][index] == trimmedName) {
			selectedMissilePresetIndex_[typeIndex] = static_cast<int>(index);
			break;
		}
	}

	missilePresetMessage_ =
		std::string(typeIndex == 0 ? "通常弾" : "ホーミング") + "設定「" + trimmedName + "」を保存しました。";
	return true;
}

bool GamePlayScene::ApplyMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName) {
	const int typeIndex = std::clamp(missileTypeIndex, 0, 1);
	const std::string trimmedName = TrimActionName(presetName);
	if (trimmedName.empty()) {
		missilePresetMessage_ = "読み込むミサイル設定名がありません。";
		return false;
	}

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		missilePresetMessage_ = "ミサイル設定ファイルが見つかりません。";
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		missilePresetMessage_ = "ミサイル設定ファイルを読み込めませんでした。";
		return false;
	}

	const char *keys[] = { "normal", "homing" };
	if (!root.contains(keys[typeIndex]) || !root[keys[typeIndex]].is_array()) {
		missilePresetMessage_ = "選択した種類の保存設定がありません。";
		return false;
	}

	const json *preset = nullptr;
	for (const auto &presetData : root[keys[typeIndex]]) {
		if (presetData.is_object() && presetData.value("name", "") == trimmedName) {
			preset = &presetData;
			break;
		}
	}
	if (!preset) {
		missilePresetMessage_ = "選択したミサイル設定が見つかりません。";
		return false;
	}

	if (typeIndex == 0) {
		missileNormalSpeed = ReadJsonFloat(*preset, "speed", missileNormalSpeed);
		missileNormalScale = ReadJsonFloat(*preset, "scale", missileNormalScale);
		missileNormalCollisionRadius = ReadJsonFloat(*preset, "collisionRadius", missileNormalCollisionRadius);
		missileNormalLifeTime = ReadJsonInt(*preset, "lifeTime", missileNormalLifeTime);
	} else {
		missileSpeed = ReadJsonFloat(*preset, "speed", missileSpeed);
		missileHomingStrength = ReadJsonFloat(*preset, "homingStrength", missileHomingStrength);
		missileHomingScale = ReadJsonFloat(*preset, "scale", missileHomingScale);
		missileHomingCollisionRadius = ReadJsonFloat(*preset, "collisionRadius", missileHomingCollisionRadius);
		missileTrailWidth = ReadJsonFloat(*preset, "trailWidth", missileTrailWidth);
		missileLifeTime = ReadJsonInt(*preset, "lifeTime", missileLifeTime);
	}
	missileMuzzleOffset = ReadJsonFloat(*preset, "muzzleOffset", missileMuzzleOffset);

	missilePresetMessage_ =
		std::string(typeIndex == 0 ? "通常弾" : "ホーミング") + "設定「" + trimmedName + "」を読み込みました。";
	return true;
}

void GamePlayScene::DrawSimulationSaveControls() {
#ifdef ENABLE_IMGUI
	ImGui::Separator();
	ImGui::InputText("保存名", simulationActionName_, IM_ARRAYSIZE(simulationActionName_));
	if (ImGui::Button("名前を付けて行動を保存")) {
		SaveNamedSimulationAction(kSimulationActionsFilePath, simulationActionName_);
	}
	ImGui::SameLine();
	if (ImGui::Button("保存一覧を更新")) {
		RefreshSimulationActionNames();
	}

	if (!simulationActionMessage_.empty()) {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.45f, 1.0f), "%s", simulationActionMessage_.c_str());
	}
#endif
}

void GamePlayScene::DrawGameplayActionControls() {
#ifdef ENABLE_IMGUI
	ImGui::Separator();
	ImGui::Text("保存済みシミュレーション設定");
	ImGui::TextWrapped("シミュレーション画面で保存した内容を、現在のゲーム側の設定値として読み込みます。");
	if (ImGui::Button("保存一覧を更新")) {
		RefreshSimulationActionNames();
	}

	if (simulationActionNames_.empty()) {
		ImGui::TextDisabled("保存されたシミュレーション設定がありません。");
		if (!simulationActionMessage_.empty()) {
			ImGui::TextWrapped("%s", simulationActionMessage_.c_str());
		}
		return;
	}

	std::vector<const char *> actionNameItems;
	actionNameItems.reserve(simulationActionNames_.size());
	for (const std::string &name : simulationActionNames_) {
		actionNameItems.push_back(name.c_str());
	}

	if (selectedSimulationActionIndex_ >= static_cast<int>(actionNameItems.size())) {
		selectedSimulationActionIndex_ = 0;
	}

	ImGui::Combo("読み込む設定", &selectedSimulationActionIndex_, actionNameItems.data(), static_cast<int>(actionNameItems.size()));
	if (ImGui::Button("この設定をゲームに読み込む")) {
		ApplySimulationAction(kSimulationActionsFilePath, simulationActionNames_[selectedSimulationActionIndex_]);
	}

	if (!simulationActionMessage_.empty()) {
		ImGui::TextWrapped("%s", simulationActionMessage_.c_str());
	}
#endif
}

void GamePlayScene::DrawMissileSettingsUI() {
#ifdef ENABLE_IMGUI
	ImGui::Text("ミサイル設定");
	ImGui::DragFloat("発射位置距離", &missileMuzzleOffset, 0.05f, 0.0f, 5.0f, "%.2f");

	ImGui::Separator();
	const char *presetTypes[] = { "通常弾", "ホーミング" };
	ImGui::Combo("保存する種類", &missilePresetTypeIndex_, presetTypes, IM_ARRAYSIZE(presetTypes));
	ImGui::InputText("ミサイル保存名", missilePresetName_, IM_ARRAYSIZE(missilePresetName_));
	if (ImGui::Button("この種類の設定を保存")) {
		SaveMissilePreset(kMissilePresetsFilePath, missilePresetTypeIndex_, missilePresetName_);
	}
	ImGui::SameLine();
	if (ImGui::Button("ミサイル保存一覧を更新")) {
		RefreshMissilePresetNames();
	}

	const int loadTypeIndex = std::clamp(missilePresetTypeIndex_, 0, 1);
	if (!missilePresetNames_[loadTypeIndex].empty()) {
		std::vector<const char *> presetItems;
		presetItems.reserve(missilePresetNames_[loadTypeIndex].size());
		for (const std::string &name : missilePresetNames_[loadTypeIndex]) {
			presetItems.push_back(name.c_str());
		}
		if (selectedMissilePresetIndex_[loadTypeIndex] >= static_cast<int>(presetItems.size())) {
			selectedMissilePresetIndex_[loadTypeIndex] = 0;
		}
		ImGui::Combo("読み込む設定", &selectedMissilePresetIndex_[loadTypeIndex], presetItems.data(), static_cast<int>(presetItems.size()));
		if (ImGui::Button("選択したミサイル設定を読み込む")) {
			ApplyMissilePreset(
				kMissilePresetsFilePath,
				loadTypeIndex,
				missilePresetNames_[loadTypeIndex][selectedMissilePresetIndex_[loadTypeIndex]]);
		}
	} else {
		ImGui::TextDisabled("%sの保存設定はまだありません。", presetTypes[loadTypeIndex]);
	}
	if (!missilePresetMessage_.empty()) {
		ImGui::TextWrapped("%s", missilePresetMessage_.c_str());
	}

	ImGui::Separator();
	ImGui::Text("通常弾（左クリック）");
	ImGui::DragFloat("通常弾速度", &missileNormalSpeed, 0.05f, 0.1f, 6.0f, "%.2f");
	ImGui::DragFloat("通常弾サイズ", &missileNormalScale, 0.01f, 0.05f, 2.0f, "%.2f");
	ImGui::DragFloat("通常弾当たり判定", &missileNormalCollisionRadius, 0.01f, 0.05f, 2.0f, "%.2f");
	ImGui::SliderInt("通常弾寿命", &missileNormalLifeTime, 10, 900);

	ImGui::Separator();
	ImGui::Text("ホーミングミサイル（右クリック）");
	ImGui::DragFloat("追尾速度", &missileSpeed, 0.05f, 0.1f, 6.0f, "%.2f");
	ImGui::DragFloat("曲がりやすさ", &missileHomingStrength, 0.005f, 0.0f, 0.6f, "%.3f");
	ImGui::DragFloat("ミサイルサイズ", &missileHomingScale, 0.01f, 0.05f, 3.0f, "%.2f");
	ImGui::DragFloat("ミサイル当たり判定", &missileHomingCollisionRadius, 0.01f, 0.05f, 3.0f, "%.2f");
	ImGui::DragFloat("煙の太さ", &missileTrailWidth, 0.01f, 0.05f, 3.0f, "%.2f");
	ImGui::SliderInt("ミサイル寿命", &missileLifeTime, 10, 1200);

	ImGui::Separator();
	if (ImGui::Button("通常弾をテスト発射")) {
		if (IsSimulationMode()) {
			isEditorPreviewPlaying_ = true;
		}
		FirePlayerMissile(MissileType::Normal);
	}
	ImGui::SameLine();
	if (ImGui::Button("ホーミングをテスト発射")) {
		if (IsSimulationMode()) {
			isEditorPreviewPlaying_ = true;
		}
		FirePlayerMissile(MissileType::MissileWithTrail);
	}
	ImGui::TextDisabled("選択中だけ確認では、弾はこのテストボタンを押した時だけ出ます。");
#endif
}

MissileTuning GamePlayScene::MakeMissileTuning(MissileType type) const {
	MissileTuning tuning;
	if (type == MissileType::Normal) {
		tuning.speed = missileNormalSpeed;
		tuning.homingStrength = 0.0f;
		tuning.scale = missileNormalScale;
		tuning.collisionRadius = missileNormalCollisionRadius;
		tuning.trailWidth = 0.0f;
		tuning.lifeTime = missileNormalLifeTime;
		return tuning;
	}

	tuning.speed = missileSpeed;
	tuning.homingStrength = missileHomingStrength;
	tuning.scale = missileHomingScale;
	tuning.collisionRadius = missileHomingCollisionRadius;
	tuning.trailWidth = missileTrailWidth;
	tuning.lifeTime = missileLifeTime;
	return tuning;
}

void GamePlayScene::FirePlayerMissile(MissileType type, Enemy *target, float horizontalOffset) {
	if (!player_ || !missileManager_) {
		return;
	}

	const MissileTuning tuning = MakeMissileTuning(type);
	const Vector3 playerPos = player_->GetPosition();
	const Vector3 forward = NormalizeOrVector3(player_->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	Vector3 right = NormalizeOrVector3(MyMath::Cross({ 0.0f, 1.0f, 0.0f }, forward), { 1.0f, 0.0f, 0.0f });
	const float muzzleOffset = (std::max)(0.0f, missileMuzzleOffset);
	const Vector3 muzzlePos = {
		playerPos.x + forward.x * muzzleOffset + right.x * horizontalOffset,
		playerPos.y + forward.y * muzzleOffset,
		playerPos.z + forward.z * muzzleOffset + right.z * horizontalOffset,
	};

	Vector3 fireDirection = forward;
	if (target && type == MissileType::MissileWithTrail) {
		Vector3 targetPosition = target->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = target->GetCollisionRadius();
		} catch (...) {}
		targetPosition.y += collisionRadius * 0.3f;
		fireDirection = NormalizeOrVector3(SubtractVector3(targetPosition, muzzlePos), forward);
	}

	const Vector3 velocity = {
		fireDirection.x * (std::max)(0.01f, tuning.speed),
		fireDirection.y * (std::max)(0.01f, tuning.speed),
		fireDirection.z * (std::max)(0.01f, tuning.speed),
	};

	missileManager_->Shoot(muzzlePos, velocity, type, tuning);
}







bool GamePlayScene::IsLockedEnemyAlive() const {
	if (!lockedEnemy_) {
		return false;
	}

	for (const auto &enemy : enemies_) {
		if (enemy.get() == lockedEnemy_) {
			try {
				if (!enemy->IsDead()) {
					return true;
				}
			} catch (...) {
				return false;
			}
		}
	}

	return false;
}

Enemy *GamePlayScene::FindLockOnTarget(Camera *activeCamera) const {
	if (!player_ || !activeCamera || player_->IsDead()) {
		return nullptr;
	}

	const Vector3 playerPosition = player_->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(player_->GetForwardVector());
	Enemy *nearestAliveEnemy = nullptr;
	float nearestAliveDistSq = (std::numeric_limits<float>::max)();
	Enemy *bestFrontEnemy = nullptr;
	float bestFrontScore = (std::numeric_limits<float>::max)();
	Enemy *bestScreenEnemy = nullptr;
	float bestScreenScore = (std::numeric_limits<float>::max)();

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	const bool hasOverlayBounds = activeCamera && GetOverlayBounds(minX, minY, maxX, maxY);
	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;

	for (const auto &enemy : enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);
		if (distSq < nearestAliveDistSq) {
			nearestAliveDistSq = distSq;
			nearestAliveEnemy = enemy.get();
		}

		const Vector3 direction = MyMath::Normalize(toEnemy);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot > 0.1f) {
			const float frontScore = distSq * 0.01f - forwardDot * 1000.0f;
			if (frontScore < bestFrontScore) {
				bestFrontScore = frontScore;
				bestFrontEnemy = enemy.get();
			}
		}

		if (hasOverlayBounds && screenWidth > 0.0f && screenHeight > 0.0f && forwardDot > -0.1f) {
			Vector3 targetPosition = enemy->GetPosition();
			float collisionRadius = 1.0f;
			try {
				collisionRadius = enemy->GetCollisionRadius();
			} catch (...) {}
			targetPosition.y += collisionRadius * 0.3f;
			Vector3 screenPosition = MyMath::WorldToScreen(
				targetPosition,
				activeCamera->GetViewProjectionMatrix(),
				screenWidth,
				screenHeight);

			if (screenPosition.z >= 0.0f && screenPosition.z <= 1.0f &&
				screenPosition.x >= 0.0f && screenPosition.x <= screenWidth &&
				screenPosition.y >= 0.0f && screenPosition.y <= screenHeight) {
				const float normalizedX = (screenPosition.x - screenWidth * 0.5f) / (screenWidth * 0.5f);
				const float normalizedY = (screenPosition.y - screenHeight * 0.5f) / (screenHeight * 0.5f);
				const float centerScore = normalizedX * normalizedX + normalizedY * normalizedY;
				const float screenScore = centerScore * 10000.0f + distSq * 0.002f - forwardDot * 150.0f;
				if (screenScore < bestScreenScore) {
					bestScreenScore = screenScore;
					bestScreenEnemy = enemy.get();
				}
			}
		}
	}

	if (bestScreenEnemy) {
		return bestScreenEnemy;
	}
	if (bestFrontEnemy) {
		return bestFrontEnemy;
	}
	return nearestAliveEnemy;
}

Enemy *GamePlayScene::FindAimAssistTarget(Camera *activeCamera) const {
	if (!player_ || !activeCamera || player_->IsDead()) {
		return nullptr;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
		return nullptr;
	}

	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
		return nullptr;
	}

	const Vector3 playerPosition = player_->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(player_->GetForwardVector());
	const float maxDistanceSq = kAimAssistMaxDistance * kAimAssistMaxDistance;
	const float centerX = screenWidth * 0.5f;
	const float centerY = screenHeight * 0.5f;
	Enemy *bestTarget = nullptr;
	float bestScore = (std::numeric_limits<float>::max)();

	for (const auto &enemy : enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);
		if (distSq > maxDistanceSq) {
			continue;
		}

		const Vector3 direction = MyMath::Normalize(toEnemy);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot <= 0.05f) {
			continue;
		}

		Vector3 targetPosition = enemy->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = enemy->GetCollisionRadius();
		} catch (...) {}
		targetPosition.y += collisionRadius * 0.3f;

		Vector3 screenPosition = MyMath::WorldToScreen(
			targetPosition,
			activeCamera->GetViewProjectionMatrix(),
			screenWidth,
			screenHeight);

		if (screenPosition.z < 0.0f || screenPosition.z > 1.0f ||
			screenPosition.x < 0.0f || screenPosition.x > screenWidth ||
			screenPosition.y < 0.0f || screenPosition.y > screenHeight) {
			continue;
		}

		const float dx = screenPosition.x - centerX;
		const float dy = screenPosition.y - centerY;
		const float screenDistanceSq = dx * dx + dy * dy;
		const float assistRadius = kAimAssistScreenRadius + collisionRadius * 12.0f;
		if (screenDistanceSq > assistRadius * assistRadius) {
			continue;
		}

		const float score = screenDistanceSq + distSq * 0.01f - forwardDot * 40.0f;
		if (score < bestScore) {
			bestScore = score;
			bestTarget = enemy.get();
		}
	}

	return bestTarget;
}

Enemy *GamePlayScene::FindMultiLockTarget(Camera *activeCamera) const {
	if (!player_ || !activeCamera || player_->IsDead() || multiLockTargets_.size() >= kMultiLockMaxTargets) {
		return nullptr;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
		return nullptr;
	}

	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
		return nullptr;
	}

	const Vector3 playerPosition = player_->GetPosition();
	const Vector3 playerForward = NormalizeOrVector3(player_->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	const float maxDistanceSq = kMultiLockMaxDistance * kMultiLockMaxDistance;
	const float centerX = screenWidth * 0.5f;
	const float centerY = screenHeight * 0.5f;
	Enemy *bestTarget = nullptr;
	float bestScore = (std::numeric_limits<float>::max)();

	for (const auto &enemy : enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }

		bool alreadyLocked = false;
		for (Enemy *target : multiLockTargets_) {
			if (target == enemy.get()) {
				alreadyLocked = true;
				break;
			}
		}
		if (alreadyLocked) {
			continue;
		}

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);
		if (distSq > maxDistanceSq) {
			continue;
		}

		const Vector3 direction = NormalizeOrVector3(toEnemy, playerForward);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot <= 0.0f) {
			continue;
		}

		Vector3 targetPosition = enemy->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = enemy->GetCollisionRadius();
		} catch (...) {}
		targetPosition.y += collisionRadius * 0.3f;

		Vector3 screenPosition = MyMath::WorldToScreen(
			targetPosition,
			activeCamera->GetViewProjectionMatrix(),
			screenWidth,
			screenHeight);

		if (screenPosition.z < 0.0f || screenPosition.z > 1.0f ||
			screenPosition.x < 0.0f || screenPosition.x > screenWidth ||
			screenPosition.y < 0.0f || screenPosition.y > screenHeight) {
			continue;
		}

		const float dx = screenPosition.x - centerX;
		const float dy = screenPosition.y - centerY;
		const float screenDistanceSq = dx * dx + dy * dy;
		const float lockRadius = kMultiLockScreenRadius + collisionRadius * 16.0f;
		if (screenDistanceSq > lockRadius * lockRadius) {
			continue;
		}

		const float score = screenDistanceSq + distSq * 0.006f - forwardDot * 80.0f;
		if (score < bestScore) {
			bestScore = score;
			bestTarget = enemy.get();
		}
	}

	return bestTarget;
}

void GamePlayScene::BeginMultiLock() {
	isMultiLockCharging_ = true;
	multiLockChargeFrames_ = 0;
	multiLockTargets_.clear();
}

void GamePlayScene::PruneMultiLockTargets() {
	auto isTargetAlive = [this](Enemy *target) {
		if (!target) {
			return false;
		}
		for (const auto &enemy : enemies_) {
			if (enemy.get() == target) {
				try {
					return !enemy->IsDead();
				} catch (...) {
					return false;
				}
			}
		}
		return false;
	};

	multiLockTargets_.erase(
		std::remove_if(
			multiLockTargets_.begin(),
			multiLockTargets_.end(),
			[&](Enemy *target) { return !isTargetAlive(target); }),
		multiLockTargets_.end());
}

void GamePlayScene::UpdateMultiLock(Camera *activeCamera) {
	if (!isMultiLockCharging_) {
		return;
	}

	PruneMultiLockTargets();
	if (multiLockTargets_.size() < kMultiLockMaxTargets &&
		(multiLockChargeFrames_ == 0 || multiLockChargeFrames_ % kMultiLockAcquireIntervalFrames == 0)) {
		if (Enemy *target = FindMultiLockTarget(activeCamera)) {
			multiLockTargets_.push_back(target);
		}
	}

	++multiLockChargeFrames_;
}

void GamePlayScene::FireMultiLockMissiles() {
	if (!isMultiLockCharging_) {
		return;
	}

	PruneMultiLockTargets();
	if (multiLockTargets_.empty()) {
		if (aimAssistEnemy_) {
			multiLockTargets_.push_back(aimAssistEnemy_);
		} else if (lockedEnemy_ && IsLockedEnemyAlive()) {
			multiLockTargets_.push_back(lockedEnemy_);
		}
	}

	if (multiLockTargets_.empty()) {
		FirePlayerMissile(MissileType::MissileWithTrail);
	} else {
		const float spacing = 0.35f;
		const float center = (static_cast<float>(multiLockTargets_.size()) - 1.0f) * 0.5f;
		for (size_t index = 0; index < multiLockTargets_.size(); ++index) {
			const float horizontalOffset = (static_cast<float>(index) - center) * spacing;
			FirePlayerMissile(MissileType::MissileWithTrail, multiLockTargets_[index], horizontalOffset);
		}
	}

	CancelMultiLock();
}

void GamePlayScene::CancelMultiLock() {
	isMultiLockCharging_ = false;
	multiLockChargeFrames_ = 0;
	multiLockTargets_.clear();
}

void GamePlayScene::UpdateLockOn(Camera *activeCamera, bool shouldUpdateGame) {
	Input *input = Input::GetInstance();
	if (!input) return;
	const bool canUseKeyboardInput = !IsImGuiKeyboardCaptureActive();

	if (canUseKeyboardInput && input->TriggerKey(DIK_TAB)) {
		lockedEnemy_ = FindLockOnTarget(activeCamera);
		aimAssistEnemy_ = nullptr;
		isCinematicLockOnCameraInitialized_ = false;
	}

	if (!IsLockedEnemyAlive()) {
		lockedEnemy_ = nullptr;
		isCinematicLockOnCameraInitialized_ = false;
	}

	aimAssistEnemy_ = nullptr;
	if (!lockedEnemy_ && shouldUpdateGame) {
		aimAssistEnemy_ = FindAimAssistTarget(activeCamera);
	}

	if (!shouldUpdateGame) {
		return;
	}

	if (canUseKeyboardInput && input->TriggerKey(DIK_X)) {
		lockedEnemy_ = nullptr;
		aimAssistEnemy_ = nullptr;
		isCinematicLockOnCameraInitialized_ = false;
		return;
	}

	if (lockedEnemy_) {
		lockedEnemy_->StartChasingPlayer();
	}
}



void GamePlayScene::Finalize() {
	if (pVoice1) {
		pVoice1->Stop();
		pVoice2->DestroyVoice();
	}

	AudioManager::GetInstance()->UnloadWave(soundData1);
	AudioManager::GetInstance()->UnloadWave(soundData2);

	// シーン刁E��替え時にポストエフェクトを通常に戻ぁE
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	EditorReceiver::GetInstance()->Finalize();
}

void GamePlayScene::Update() {

	// BlenderからチE�Eタが来てぁE��ら敵をリアルタイム更新�E�E
	if (EditorReceiver::GetInstance()->Update(player_.get(), enemies_, obstacles_, enemySpawns_)) {
		for (auto &spawnData : enemySpawns_) {
			if (spawnData.HasReinforcementTrigger() || enemyEventManager_.IsTargetEnemy(spawnData.name)) {
				spawnData.isInitialSpawn = false;
			}
		}
// 		SpawnEnemiesFromSpawnPoints();
	}


	// =========================================================
	// ホットリロード�E監視�E琁E��E
	// =========================================================
	try {
		// 今�E "scene.json" の更新日時をチェチE��する
		auto currentTime = std::filesystem::last_write_time("resources/scene.json");

		// もし記�EしてぁE��日時よりも新しけれ�E�E�＝Blenderで上書き保存されたら！E��E
		if (currentTime > lastJsonWriteTime_) {
			ReloadSceneJson();

			// チE��チE��ウィンドウにお知らせを�EぁE
			OutputDebugStringA("Hot Reloaded: scene.json を�E読み込みしました�E�\n");
		}
	} catch (...) {
		// 💡趁E��要E��Blenderがファイルに書き込んでぁE��最中�E�数ミリ秒）�E
		// C++からアクセスできずエラーになることがあるため、try-catchで握りつぶぁE
	}

	const bool canUseKeyboardInput = !IsImGuiKeyboardCaptureActive();
	const bool canUseMouseInput = !IsImGuiMouseCaptureActive();

	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_0)) {
		OutputDebugStringA("HIt 0\n");
	}

	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_F2)) {
		if (IsSimulationMode()) {
			PostQuitMessage(0);
		} else {
			LaunchSimulationExecutable();
		}
		return;
	}

	if (canUseKeyboardInput && IsSimulationMode() && Input::GetInstance()->TriggerKey(DIK_F3)) {
		SetDebugCameraActive(!isDebugCameraActive_);
	}

	// Rキーでシーンを最初からやり直ぁE
	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_R)) {
		SceneManager::GetInstance()->ChangeScene(IsSimulationMode() ? "SIMULATION" : "GAMEPLAY");
		return;
	}

	// ==========================================
	// ゲームオーバ�E判定と演�E進衁E
	// ==========================================
	//if (!isGameOver_ && player_ && player_->IsDead()) {
	//	isGameOver_ = true;
	//	gameOverTimer_ = 0;

	//	// 💥 自機がめE��れた時�E大爁E��パ�EチE��クルを生成！E
	//	std::vector<Vector3> playerHitPos = { player_->GetPosition() };
	//	if (explosionManager_) {
	//		explosionManager_->CreateExplosions(playerHitPos);
	//	}

	//	// 🎵 BGMを停止して絶望感を演�E
	//	if (pVoice2) {
	//		pVoice2->Stop();
	//	}
	//}

	bool shouldUpdateGame = true;

	if (isGameOver_) {
		gameOverTimer_++;

		// 絶望�E白黒化�E�グレースケール�E�エフェクトを適用�E�E
		if (PostEffect::GetInstance()) {
			float effectProgress = static_cast<float>(gameOverTimer_) / 120.0f;
			if (effectProgress > 1.0f) {
				effectProgress = 1.0f;
			}
			float vignetteRadius = 0.62f - 0.22f * effectProgress;
			float blurIntensity = 1.5f + 3.0f * effectProgress;
			PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.38f, blurIntensity);
		}

		// 5フレームに1回だけ更新することで、スローモーション�E�世界停止�E�を実現�E�E
		shouldUpdateGame = (gameOverTimer_ % 5 == 0);

		// 紁E秒！E20フレーム�E�経過したら、正式にゲームオーバ�Eシーンへ遷移する�E�E
		if (gameOverTimer_ >= 120) {
			SceneManager::GetInstance()->ChangeScene("GAMEOVER");
		}
	} else {
		// 通常時�Eノ�EマルエフェクチE
		if (PostEffect::GetInstance()) {
			PostEffect::GetInstance()->SetEffectType(0); // 0: Normal
		}
	}
	shouldUpdateGame = shouldUpdateGame && isEditorPreviewPlaying_;
	const bool isSimulation = IsSimulationMode();
	const bool isFullFlowPreview = !isSimulation || simulationPlaybackMode_ == 1;
	const bool isSelectedOnlyPreview = isSimulation && simulationPlaybackMode_ == 0;
	const bool updateSelectedPlayer = shouldUpdateGame && canUseKeyboardInput && (isFullFlowPreview || currentSimulationTarget_ == 0);
	const bool updateSelectedMissiles = shouldUpdateGame && (isFullFlowPreview || currentSimulationTarget_ == 1);
	const bool updateSelectedEnemies = shouldUpdateGame && (isFullFlowPreview || currentSimulationTarget_ == 2);
	const bool updateSelectedParticles = shouldUpdateGame && (isFullFlowPreview || currentSimulationTarget_ == 3);
	const bool allowMouseMissileFire = shouldUpdateGame && canUseMouseInput && (!isSimulation || isFullFlowPreview);
	const bool allowLockOnBehavior = !isGameOver_ && (isFullFlowPreview || currentSimulationTarget_ == 1 || currentSimulationTarget_ == 2);
	const bool updateDebugWireframes = !isSimulation || isFullFlowPreview || updateSelectedPlayer || updateSelectedMissiles || updateSelectedEnemies || updateSelectedParticles;
	const bool updateAnimationPreview = !isSimulation || isFullFlowPreview;

	if (myBox && animationData.duration > 0.0f) {
		if (playAnimation && updateAnimationPreview) {
			animationTime += 1.0f / 60.0f;
			animationTime = std::fmod(animationTime, animationData.duration);
		}
		
		// アニメーションの更新と骨への適用
		ApplyAnimation(skeleton, animationData, animationTime);
		::Update(skeleton);
		if (enableSkinning && myModelObject->GetModel()) {
			myModelObject->GetModel()->UpdateSkinCluster(myModelObject->skinCluster, skeleton);
		}

		// 今�EめE��に合わせた状態で使ぁE��め、Skeletonから計算結果を取り�EしてBox/Modelに適用する
		if (!skeleton.joints.empty()) {
			myBox->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
			myBox->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
			myBox->SetScale(skeleton.joints[skeleton.root].transform.scale);

			// スキニングが実裁E��れたため、スキンなしModel��の場合�EみTransformを適用する
			// スキニングが実裁E��れたため、スキンなしModel��の場合�EみTransformを適用する
			if (myModelObject->GetModel()) {
				if (!myModelObject->skinCluster.isValid) {
					myModelObject->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
					myModelObject->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
					myModelObject->SetScale(skeleton.joints[skeleton.root].transform.scale);
				} else {
					// スキニングModel��はアニメーションが行�Eに含まれるため、�EースのトランスフォームはリセチE��する
					// (これを行わなぁE��二重に移動して画面外に消えめE
					myModelObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
					myModelObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
					myModelObject->SetScale({ modelScale, modelScale, modelScale });
				}
			}
		}

		// 骨描画の更新
		if (showBones && updateAnimationPreview) {
			std::vector<VertexData> lineVertices;

			for (size_t i = 0; i < skeleton.joints.size(); ++i) {
				Vector3 pos = {
					skeleton.joints[i].skeletonSpaceMatrix.m[3][0],
					skeleton.joints[i].skeletonSpaceMatrix.m[3][1],
					skeleton.joints[i].skeletonSpaceMatrix.m[3][2]
				};
				// Model��全体�Eスケールに合わせてボ�Eンの座標もスケーリングする
				pos.x *= modelScale;
				pos.y *= modelScale;
				pos.z *= modelScale;

				// ライン用の頂点を作�E�E�親がいる場合！E
				if (skeleton.joints[i].parent) {
					int32_t parentIndex = *skeleton.joints[i].parent;
					Vector3 parentPos = {
						skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][0],
						skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][1],
						skeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][2]
					};
					parentPos.x *= modelScale;
					parentPos.y *= modelScale;
					parentPos.z *= modelScale;

					VertexData v1, v2;
					v1.position = { parentPos.x, parentPos.y, parentPos.z, 1.0f };
					v1.normal = { 0.0f, 1.0f, 0.0f };
					v1.texcoord = { 0.0f, 0.0f };
					v1.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色

					v2.position = { pos.x, pos.y, pos.z, 1.0f };
					v2.normal = { 0.0f, 1.0f, 0.0f };
					v2.texcoord = { 1.0f, 1.0f };
					v2.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色

					lineVertices.push_back(v1);
					lineVertices.push_back(v2);
				}
			}

			// ラインModel��の頂点を更新
			if (!lineVertices.empty() && skeletonLinesObject->GetModel()) {
				skeletonLinesObject->GetModel()->UpdateLineVertices(lineVertices);
			}
			skeletonLinesObject->Update();
		}
	}

	// Model��の更新
	if (showModel && myModelObject) {
		myModelObject->Update();
	}

	// Debug camera switching is handled by ImGui buttons in UpdateUI().
	if (false && Input::GetInstance()->TriggerKey(DIK_F1)) {
		SetDebugCameraActive(!isDebugCameraActive_);
	}

	// プレイヤーの更新と、カメラの追征E
	if (player_) {
		if (updateSelectedPlayer) {
			if (lockedEnemy_) {
				player_->UpdateLockOnRotation(lockedEnemy_->GetPosition());
			}
			player_->Update(obstacles_);
		} else {
			player_->UpdateModel();
		}

	}

	// ==========================================
	// 敵
	// ==========================================
	// プレイヤーの最新座標を取得すめE
	Vector3 playerPos = player_ ? player_->GetOBB().center : Vector3{ 0.0f, 0.0f, 0.0f };

	if (updateSelectedEnemies) {
		// 敵の弾の更新�E�被弾時�E爁E��座標を受け取る�E�E
		std::vector<Vector3> enemyBulletHits;
		if (enemyBulletManager_ && player_) {
			enemyBulletManager_->Update(player_.get(), enemyBulletHits, obstacles_);
		}

		// 敵の弾が�Eレイヤーに当たった場合も爁E��を発生させる
		if (explosionManager_ && !enemyBulletHits.empty()) {
			explosionManager_->CreateExplosions(enemyBulletHits);
		}

		for (auto it = enemies_.begin(); it != enemies_.end(); ) {
			(*it)->Update(playerPos, enemyBulletManager_.get(), obstacles_);
			if ((*it)->IsDead()) {
				if (lockedEnemy_ == it->get()) {
					lockedEnemy_ = nullptr;
					isCinematicLockOnCameraInitialized_ = false;
				}
				if (aimAssistEnemy_ == it->get()) {
					aimAssistEnemy_ = nullptr;
				}
				if (missileManager_) {
// 					missileManager_->ClearTarget(it->get());
				}
				size_t spawnPointIndex = (*it)->GetSpawnPointIndex();
				
				if (spawnPointIndex < enemySpawns_.size()) {
					const std::string& deadName = enemySpawns_[spawnPointIndex].name;
// 					TriggerEnemyReinforcements(deadName);
				}

				if (spawnPointIndex < enemySpawns_.size() && enemySpawns_[spawnPointIndex].isInitialSpawn) {
// 					ScheduleEnemySpawn(spawnPointIndex, kEnemyRespawnDelayFrames);
				}
				it = enemies_.erase(it); // 当たった敵はリストから消滁E
			} else {
				++it;
			}
		}
// 		UpdateEnemyRespawns();

		// 障害物自身のUpdateを回す（現状中身は空に近いですが一応回します！E
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	} else {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
	}

	// カメラの更新
	if (isDebugCameraActive_) {
		if (canUseKeyboardInput && canUseMouseInput) {
			debugFlyCamera_->Update(); // FlyCameraが�E刁E��入力を消化して自刁E��更新する
		} else {
			debugFlyCamera_->Camera::Update();
		}
		skybox->Update(debugFlyCamera_.get());
	} else {
		if (player_) {
			Vector3* targetPos = nullptr;
			Vector3 enemyPos;
			if (lockedEnemy_) {
				enemyPos = lockedEnemy_->GetPosition();
				targetPos = &enemyPos;
			}
			player_->UpdateCamera(camera.get(), targetPos);
		}
		camera->Update();
		skybox->Update(camera.get());
	}

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	if (isSelectedOnlyPreview) {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
		if (enemyBulletManager_) {
			enemyBulletManager_->UpdateModels();
		}
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	}

	if (myRing && showNormalRing) {
		static float ringTime = 0.0f;
		ringTime += 0.05f;

		// オブジェクト�E体�E回転めE��縮は行わず、UVスクロールのみでエフェクトを表現する
		// パ�EチE��クルのエフェクトを囲むようにスケールを調整
		myRing->SetRotate({ 0.0f, 0.0f, 0.0f });
		myRing->SetScale({ 2.0f, 2.0f, 1.0f });

		// UVスクロールとスケーリング
		Model* ringModel = myRing->GetModel();
		if (ringModel) {
			Vector3 uvScale = { 10.0f, 1.0f, 1.0f }; // U方向にScaleして細かい模様にする
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			// 賁E��の持E��通り、U方向！E成�E�E�を時間でスクロールさせて冁E��回転させめE
			Vector3 uvTranslate = { ringTime * 0.1f, 0.0f, 0.0f }; 
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			ringModel->SetUvTransform(uvTransform);
		}
		myRing->Update();
	}

	if (myPartialRing && showPartialRing) {
		static float pRingTime = 0.0f;
		pRingTime += 0.05f;

		// 部刁E��ングはV方向をスクロールさせたり、Z軸回転させたりしてアニメーションできる
		myPartialRing->SetRotate({ 0.0f, 0.0f, pRingTime * -0.5f }); // Z回転で三日月を回す
		myPartialRing->SetScale({ 2.0f, 2.0f, 1.0f });

		Model* pRingModel = myPartialRing->GetModel();
		if (pRingModel) {
			Vector3 uvScale = { 1.0f, 10.0f, 1.0f };
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			Vector3 uvTranslate = { 0.0f, pRingTime * 0.1f, 0.0f }; // V方向へスクロール
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			pRingModel->SetUvTransform(uvTransform);
		}
		myPartialRing->Update();
	}

	if (myCylinder && showCylinder) {
		cylinderUVOffset[0] += cylinderUVScrollSpeed[0];
		cylinderUVOffset[1] += cylinderUVScrollSpeed[1];

		Model* cModel = myCylinder->GetModel();
		if (cModel) {
			Vector3 uvScale = { 1.0f, 1.0f, 1.0f };
			Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
			Vector3 uvTranslate = { cylinderUVOffset[0], cylinderUVOffset[1], 0.0f };
			
			MyMath math;
			Matrix4x4 uvTransform = math.MakeAffineMatrix(uvScale, uvRotate, uvTranslate);
			cModel->SetUvTransform(uvTransform);
			cModel->SetAlphaReference(cylinderAlphaReference);
		}
		
		// ImGuiの変数をCylinderに適用
		myCylinder->SetTranslate({ cylinderPos[0], cylinderPos[1], cylinderPos[2] });
		myCylinder->SetScale({ cylinderScale[0], cylinderScale[1], cylinderScale[2] });

		myCylinder->Update();
	}

	for (Object3d *object3d : objects) {
		object3d->Update();
	}

	Vector2 size = sprite->GetSize();
	size.x = 300.0f;
	size.y = 300.0f;
	sprite->SetSize(size);

	UpdateLockOn(activeCamera, allowLockOnBehavior);

	if (showParticles && updateSelectedParticles) {
		particleEmitter->Update();
	}


	// ==========================================
	// ミサイルの発封E�E琁E
	// ==========================================
	if (allowMouseMissileFire && player_ && !isGameOver_) {
		Input *input = Input::GetInstance();
		// 左クリチE���E�速くて煙が出なぁE��常弾
		if (input->TriggerMouseButton(0)) {
			FirePlayerMissile(MissileType::Normal);
		}

		// 右クリチE���E��Eを引きながら敵へ曲がるホ�Eミング弾
		if (input->TriggerMouseButton(1)) {
			BeginMultiLock();
		}
		if (isMultiLockCharging_ && input->PushMouseButton(1)) {
			UpdateMultiLock(activeCamera);
		}
		if (isMultiLockCharging_ && !input->PushMouseButton(1)) {
			FireMultiLockMissiles();
		}
	} else if (isMultiLockCharging_) {
		CancelMultiLock();
	}

	// ==========================================
	// 弾の更新処琁E
	// ==========================================
	std::vector<Vector3> hitPositions;
	if (updateSelectedMissiles) {
		if (missileManager_) {
			missileManager_->Update(activeCamera, enemies_, obstacles_, hitPositions, lockedEnemy_);
		}

		// 爁E��マネージャーに座標リストを渡して、発生を依頼するだけ！E
		if (explosionManager_ && !hitPositions.empty()) {
			explosionManager_->CreateExplosions(hitPositions);
		}

	} else {
		if (missileManager_) {
			missileManager_->UpdateModels(activeCamera);
		}
	}

	// 爁E��マネージャーの更新
	if ((!isSimulation || updateSelectedMissiles || updateSelectedParticles || (shouldUpdateGame && isFullFlowPreview)) && explosionManager_) {
		explosionManager_->Update();
	}

	// 大允E�Eパ�EチE��クル全体�E更新
	if (!isSimulation || updateSelectedMissiles || updateSelectedParticles || (shouldUpdateGame && isFullFlowPreview)) {
		particleManager->Update(activeCamera);
	}

	// ==========================================
	// チE��チE��用コライダー頂点構篁E
	// ==========================================
	if (showDebugColliders && updateDebugWireframes && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		std::vector<VertexData> colliderVertices;
		const bool drawAllDebugFrames = !isSelectedOnlyPreview || isFullFlowPreview;
		const bool drawPlayerDebugFrame = drawAllDebugFrames || currentSimulationTarget_ == 0;
		const bool drawMissileDebugFrame = drawAllDebugFrames || currentSimulationTarget_ == 1;
		const bool drawEnemyDebugFrame = drawAllDebugFrames || currentSimulationTarget_ == 2;
		const bool drawObstacleDebugFrame = drawAllDebugFrames;

		auto pushLine = [&](const Vector3& start, const Vector3& end, const Vector4& color) {
			VertexData v1{};
			VertexData v2{};
			v1.position = { start.x, start.y, start.z, 1.0f };
			v2.position = { end.x, end.y, end.z, 1.0f };
			v1.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
			v2.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
			v1.texcoord = { 0.0f, 0.0f, 0.0f, 0.0f };
			v2.texcoord = { 1.0f, 1.0f, 0.0f, 0.0f };
			v1.color = color;
			v2.color = color;
			colliderVertices.push_back(v1);
			colliderVertices.push_back(v2);
		};

		auto addAABB = [&](const Vector3& center, const Vector3& extents, const Vector4& color) {
			Vector3 p[8] = {
				{ center.x - extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z + extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z + extents.z },
			};

			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addOBB = [&](const Vector3& center, const Vector3& extents, const Vector3& rotation, const Vector4& color) {
			Vector3 local[8] = {
				{ -extents.x, -extents.y, -extents.z },
				{  extents.x, -extents.y, -extents.z },
				{  extents.x,  extents.y, -extents.z },
				{ -extents.x,  extents.y, -extents.z },
				{ -extents.x, -extents.y,  extents.z },
				{  extents.x, -extents.y,  extents.z },
				{  extents.x,  extents.y,  extents.z },
				{ -extents.x,  extents.y,  extents.z },
			};
			const Matrix4x4 rotationMatrix = MyMath::Multiply(
				MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation.x), MyMath::MakeRotateYMatrix(rotation.y)),
				MyMath::MakeRotateZMatrix(rotation.z));

			Vector3 p[8]{};
			for (int i = 0; i < 8; ++i) {
				const Vector3 rotated = MyMath::Transform(local[i], rotationMatrix);
				p[i] = AddVector3(center, rotated);
			}

			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addOBBShape = [&](const OBB& obb, const Vector4& color) {
			Vector3 p[8] = {
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
			};
			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addSphere = [&](const Vector3& center, float radius, const Vector4& color) {
			constexpr int segmentCount = 24;
			constexpr float twoPi = 6.283185307f;
			for (int i = 0; i < segmentCount; ++i) {
				const float angle1 = twoPi * static_cast<float>(i) / static_cast<float>(segmentCount);
				const float angle2 = twoPi * static_cast<float>(i + 1) / static_cast<float>(segmentCount);
				const float cos1 = std::cos(angle1);
				const float sin1 = std::sin(angle1);
				const float cos2 = std::cos(angle2);
				const float sin2 = std::sin(angle2);

				pushLine(
					{ center.x + radius * cos1, center.y + radius * sin1, center.z },
					{ center.x + radius * cos2, center.y + radius * sin2, center.z },
					color);
				pushLine(
					{ center.x, center.y + radius * cos1, center.z + radius * sin1 },
					{ center.x, center.y + radius * cos2, center.z + radius * sin2 },
					color);
				pushLine(
					{ center.x + radius * sin1, center.y, center.z + radius * cos1 },
					{ center.x + radius * sin2, center.y, center.z + radius * cos2 },
					color);
			}
		};

		// 1. プレイヤーのAABBと琁E
		if (drawPlayerDebugFrame && player_ && !player_->IsDead()) {
			addOBBShape(player_->GetOBB(), { 0.0f, 1.0f, 0.0f, 1.0f });
		}

		// 2. 障害物のAABB
		if (drawObstacleDebugFrame) {
			for (const auto& obstacle : obstacles_) {
				if (!obstacle || obstacle->IsStageBounds()) {
					continue;
				}
				// Model��の実際のバウンチE��ングボックス ÁEBlenderスケール = 正確なワールドAABB
				addOBBShape(obstacle->GetOBB(), { 0.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		// 3. 敵のAABBと琁E
		if (drawEnemyDebugFrame) {
			for (const auto& enemy : enemies_) {
				if (!enemy->IsDead()) {
					addOBBShape(enemy->GetOBB(), { 1.0f, 0.0f, 0.0f, 1.0f });
				}
			}
		}

		if ((drawEnemyDebugFrame || drawMissileDebugFrame) && lockedEnemy_ && !lockedEnemy_->IsDead()) {
			addSphere(lockedEnemy_->GetPosition(), lockedEnemy_->GetCollisionRadius() + 0.35f, { 1.0f, 0.95f, 0.0f, 1.0f });
		}

		// 4. 自機ミサイル�E�Elayer Bullets�E�E
		if (drawMissileDebugFrame && missileManager_) {
			for (const auto& missile : missileManager_->GetMissiles()) {
				if (!missile->IsDead()) {
					// Sphere: Magenta (radius: 0.5f)
					addSphere(missile->GetPosition(), missile->GetCollisionRadius(), { 1.0f, 0.0f, 1.0f, 1.0f });
				}
			}
		}

		// 5. 敵の弾�E�Enemy Bullets�E�E
		if (drawEnemyDebugFrame && enemyBulletManager_) {
			for (const auto& bullet : enemyBulletManager_->GetBullets()) {
				if (!bullet.isDead) {
					// Sphere: Orange (radius: 0.5f)
					addSphere(bullet.position, 0.5f, { 1.0f, 0.5f, 0.0f, 1.0f });
				}
			}
		}

		// 空の場合�Eダミ�Eの透�Eな線を追加�E�リソース stuck 防止�E�E
		if (colliderVertices.empty()) {
			VertexData v1, v2;
			v1.position = { 0.0f, 0.0f, 0.0f, 1.0f };
			v1.color = { 0.0f, 0.0f, 0.0f, 0.0f };
			v2.position = { 0.0f, 0.0f, 0.0f, 1.0f };
			v2.color = { 0.0f, 0.0f, 0.0f, 0.0f };
			colliderVertices.push_back(v1);
			colliderVertices.push_back(v2);
		}

		debugColliderLinesObject->GetModel()->UpdateLineVertices(colliderVertices);
		debugColliderLinesObject->Update();
	}

#ifdef ENABLE_IMGUI
	UpdateUI();
#endif
	sprite->Update();
}

void GamePlayScene::Draw() {
	//3Dオブジェト�E描画準備
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// プレイヤーの描画
	if (player_) {
		player_->Draw();
	}

	// すべてのミサイルを描画
	if (missileManager_) {
		missileManager_->Draw();
	}

	// 敵の弾を描画
	if (enemyBulletManager_) {
		enemyBulletManager_->Draw();
	}

	// 敵の描画
	for (const auto &enemy : enemies_) {
		enemy->Draw();
	}

	Vector4 frustumPlanes[6];
	MyMath::ExtractFrustumPlanes(camera->GetViewProjectionMatrix(), frustumPlanes);

	// 障害物の描画
	for (const auto &obstacle : obstacles_) {
		Sphere obsSphere;
		obsSphere.center = obstacle->GetPosition();
		obsSphere.radius = MyMath::Length(obstacle->GetWorldHalfExtents());

		// 画面外�E場合�E描画しなぁE��カリング�E�E
		if (MyMath::IsInFrustum(obsSphere, frustumPlanes)) {
			obstacle->Draw();
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	//3Dオブジェクト�E描画
	if (showPlane) {
		for (Object3d* object3d : objects) {
			object3d->Draw();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// アニメーションModel��の個別描画制御
	if (showModel && myModelObject) {
		myModelObject->Draw();
	}
	
	if (showBones) {
		// ボ�Eン描画の前に設定を確実にする
		Object3dCommon::GetInstance()->SetCommonDrawSettings();

		// ボ�Eンラインの描画
		if (skeletonLinesObject && skeletonLinesObject->GetModel()) {
			skeletonLinesObject->Draw();
		}
	}

	if (showDebugColliders && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		debugColliderLinesObject->Draw();
	}

	if (showSkybox && skybox) {
		skybox->Draw();
	}
	
	// エフェクト系の描画 (深度書き込み無効)
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (myRing && showNormalRing) myRing->Draw();
	if (myPartialRing && showPartialRing) myPartialRing->Draw();
	if (myCylinder && showCylinder) myCylinder->Draw();
	if (explosionManager_) explosionManager_->Draw();


	//Spriteの描画基溁E
	SpriteCommon::GetInstance()->SetCommonPipelineState();
	//スプライト描画
	if (showSprite) {
		sprite->Draw();
	}

	DrawOverlay();

	particleManager->Draw();
}

void GamePlayScene::DrawOverlay() {
	if (isDebugCameraActive_ && !debugFlyCamera_) return;
	if (!isDebugCameraActive_ && !camera) return;

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	if (!activeCamera) return;

	if (isMultiLockCharging_ && !multiLockTargets_.empty()) {
		for (Enemy *target : multiLockTargets_) {
			DrawLockOnOverlaySprite(target, activeCamera->GetViewProjectionMatrix(), lockOnReticleSprite_.get());
		}
	} else if (Enemy *overlayTarget = lockedEnemy_ ? lockedEnemy_ : aimAssistEnemy_) {
		DrawLockOnOverlaySprite(overlayTarget, activeCamera->GetViewProjectionMatrix(), lockOnReticleSprite_.get());
	} else {
		DrawAimCursorOverlaySprite(aimCursorSprite_.get());
	}
}



void GamePlayScene::DrawSimulationScreenUI() {
#ifdef ENABLE_IMGUI
	const ImVec2 windowSize(
		static_cast<float>(WinApp::GetClientWidth()),
		static_cast<float>(WinApp::GetClientHeight()));
	float panelWidth = (windowSize.x > 560.0f) ? 520.0f : windowSize.x - 40.0f;
	if (panelWidth < 320.0f) {
		panelWidth = 320.0f;
	}
	float panelHeight = windowSize.y - 40.0f;
	if (panelHeight < 360.0f) {
		panelHeight = 360.0f;
	}
	float panelX = windowSize.x - panelWidth - 20.0f;
	if (panelX < 20.0f) {
		panelX = 20.0f;
	}
	ImGui::SetNextWindowPos(ImVec2(panelX, 20.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

	const ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse;

	if (!ImGui::Begin("シミュレーション画面", nullptr, windowFlags)) {
		ImGui::End();
		return;
	}

	ImGui::Text("シミュレーション画面");
	ImGui::SameLine();
	if (ImGui::Button("シミュレーションを閉じる (F2)")) {
		PostQuitMessage(0);
		ImGui::End();
		return;
	}
	ImGui::SameLine();
	if (ImGui::Button("リセット")) {
		ResetEditorPreview();
	}
	ImGui::SameLine();
	if (ImGui::Button("再生")) {
		isEditorPreviewPlaying_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("停止")) {
		isEditorPreviewPlaying_ = false;
	}
	ImGui::SameLine();
	ImGui::TextColored(
		isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
		"状態: %s",
		isEditorPreviewPlaying_ ? "再生中" : "停止中");

	ImGui::Separator();
	const char *previewModes[] = { "選択中だけ確認", "全体確認" };
	ImGui::Combo("確認モード", &simulationPlaybackMode_, previewModes, IM_ARRAYSIZE(previewModes));
	if (simulationPlaybackMode_ == 0) {
		ImGui::TextDisabled("今選んでいるカテゴリだけ動きます。ミサイルはテスト発射ボタンでだけ出ます。");
	} else {
		ImGui::TextDisabled("プレイヤー・敵・ミサイルをまとめて動かして全体の流れを確認します。");
	}

	ImGui::Separator();
	ImGui::Text("カメラ: %s", isDebugCameraActive_ ? "フリーカメラ" : "プレイヤー視点");
	ImGui::SameLine();
	if (isDebugCameraActive_) {
		if (ImGui::Button("プレイヤー視点にする (F3)")) {
			SetDebugCameraActive(false);
		}
	} else {
		if (ImGui::Button("フリーカメラに戻す (F3)")) {
			SetDebugCameraActive(true);
		}
	}

	DrawSimulationSaveControls();

	ImGui::Separator();
	const char *categories[] = { "プレイヤー", "ミサイル", "敵 & イベント", "パーティクル", "カメラ" };
	ImGui::Combo("カテゴリ", &currentSimulationTarget_, categories, IM_ARRAYSIZE(categories));
	ImGui::Separator();

	if (currentSimulationTarget_ == 0) {
		ImGui::Text("プレイヤー移動設定");
		if (player_) {
			auto mode = player_->GetCurrentMode();
			const char* modeName = (mode == PlayerMode::Fighter) ? "ファイター (1キー)" : 
								   (mode == PlayerMode::Gerwalk) ? "ガウォーク (2キー)" : "バトロイド (3キー)";
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "現在の形態: %s", modeName);
			
			PlayerModeParams& p = player_->GetModeParams(mode);
			ImGui::SliderFloat("最大移動速度", &p.maxMoveSpeed, 0.01f, 1.0f);
			ImGui::SliderFloat("移動加速度", &p.moveAcceleration, 0.001f, 0.1f);
			ImGui::SliderFloat("移動減衰", &p.moveDamping, 0.1f, 1.0f);
			ImGui::SliderFloat("ピッチ回転速度", &p.pitchSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ヨー回転速度", &p.yawSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ロール回転速度", &p.rollSpeed, 0.001f, 0.1f);

			ImGui::Separator();
			ImGui::Text("アニメーションデバッグ");
			bool isDebug = player_->IsAnimDebugActive();
			if (ImGui::Checkbox("デバッグ時間を強制", &isDebug)) {
				player_->SetAnimDebugActive(isDebug);
			}
			if (isDebug) {
				float t = player_->GetTargetAnimationTime();
				float duration = player_->GetAnimationDuration();
				if (ImGui::SliderFloat("アニメーション時間 (秒)", &t, 0.0f, duration, "%.3f秒")) {
					player_->SetTargetAnimationTime(t);
				}
				ImGui::Text("対応フレーム (24fps換算): %.1f", t * 24.0f);
			} else {
				ImGui::Text("アニメーション時間: %.3f秒 (目標: %.3f秒)", player_->GetAnimationTime(), player_->GetTargetAnimationTime());
			}
		} else {
			ImGui::Text("プレイヤーが初期化されていません。");
		}
	} else if (currentSimulationTarget_ == 1) {
		DrawMissileSettingsUI();
	} else if (currentSimulationTarget_ == 2) {
		ImGui::Text("=== 敵の出現とルート ===");
		ImGui::Text("Lock-on: %s", lockedEnemy_ ? "LOCKED" : "NONE");
		ImGui::Text("Tab: ターゲットロック / X: ロック解除 / F2: シミュレーションを閉じる");
		ImGui::DragFloat3("出現座標 (X,Y,Z)", newEnemyPos, 1.0f);

		if (ImGui::Button("敵を生成する！")) {
			auto newEnemy = std::make_unique<Enemy>();
			newEnemy->Initialize({ newEnemyPos[0], newEnemyPos[1], newEnemyPos[2] });
			enemies_.push_back(std::move(newEnemy));
		}

		ImGui::Separator();
		ImGui::Text("敵のリスト (総数: %d)", static_cast<int>(enemies_.size()));
		int index = 0;
		for (const auto &enemy : enemies_) {
			Vector3 pos = enemy->GetPosition();
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
			++index;
		}

		ImGui::Separator();
		ImGui::Text("イベントツール（増援設定）");
		static int selectedTrigger = 0;
		static int selectedTarget = 0;
		static int eventDelay = 60;
		std::vector<const char *> enemyNames;
		for (const auto &spawn : enemySpawns_) {
			enemyNames.push_back(spawn.name.c_str());
		}

		if (!enemyNames.empty()) {
			if (selectedTrigger >= static_cast<int>(enemyNames.size())) {
				selectedTrigger = 0;
			}
			if (selectedTarget >= static_cast<int>(enemyNames.size())) {
				selectedTarget = 0;
			}

			ImGui::Combo("トリガーとなる敵", &selectedTrigger, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::Combo("出現する敵(ターゲット)", &selectedTarget, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::DragInt("出現までのディレイ(フレーム)", &eventDelay, 1, 0, 600);
			if (ImGui::Button("イベントを追加")) {
				enemyEventManager_.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
				enemySpawns_[selectedTarget].isInitialSpawn = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("イベントを保存")) {
				enemyEventManager_.SaveEvents("resources/enemy_events.json");
			}

			for (size_t i = 0; i < enemyEventManager_.GetEvents().size(); ++i) {
				const auto &event = enemyEventManager_.GetEvents()[i];
				ImGui::Text("[%d] %s が死んだら %d F後に %s が出現",
					static_cast<int>(i),
					event.triggerEnemyName.c_str(),
					event.delayFrames,
					event.targetEnemyName.c_str());
				ImGui::SameLine();
				if (ImGui::Button(("削除##event" + std::to_string(i)).c_str())) {
					enemyEventManager_.RemoveEvent(i);
					break;
				}
			}
		} else {
			ImGui::Text("敵の出現データがありません。");
		}
	} else if (currentSimulationTarget_ == 3) {
		ImGui::Text("=== GPUパーティクル ===");
		bool gpuChanged = false;
		if (particleManager) {
			if (auto *emitter = particleManager->GetEmitterSphere()) {
				if (ImGui::DragFloat3("位置", &emitter->translate.x, 0.01f)) gpuChanged = true;
				if (ImGui::DragFloat("射出半径", &emitter->radius, 0.01f)) gpuChanged = true;
				if (ImGui::DragInt("射出数", reinterpret_cast<int *>(&emitter->count), 1, 0, 1000)) gpuChanged = true;
				if (ImGui::DragFloat("射出間隔", &emitter->frequency, 0.01f, 0.01f, 10.0f)) gpuChanged = true;
			}
			if (ImGui::Button("GPUパーティクルを再初期化") || gpuChanged) {
				particleManager->RequestGpuInitialize();
			}
		}

		ImGui::Separator();
		ImGui::Text("=== 爆発パーティクル ===");
		if (explosionManager_) {
			auto &config = explosionManager_->GetConfig();
			ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
			ImGui::ColorEdit4("カラー", config.color);
			ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f);
			ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f);
			ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f);
			if (ImGui::Button("設定をJSONに保存")) {
				explosionManager_->SaveToJson("resources/explosionConfig.json");
			}
			ImGui::SameLine();
			if (ImGui::Button("設定をJSONから読込")) {
				explosionManager_->LoadFromJson("resources/explosionConfig.json");
			}
		}
	} else if (currentSimulationTarget_ == 4) {
		ImGui::Text("カメラ設定");
		if (isDebugCameraActive_) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[フリーカメラ アクティブ]");
			if (ImGui::Button("プレイヤー視点にする")) {
				SetDebugCameraActive(false);
			}
			float moveSpeed = debugFlyCamera_->GetMoveSpeed();
			float rotateSpeed = debugFlyCamera_->GetRotateSpeed();
			float sensitivity = debugFlyCamera_->GetMouseSensitivity();
			float scrollSpeed = debugFlyCamera_->GetScrollSpeed();
			float panSpeed = debugFlyCamera_->GetPanSpeed();
			if (ImGui::DragFloat("移動速度 (WASD)##fly", &moveSpeed, 0.01f, 0.01f, 20.0f)) debugFlyCamera_->SetMoveSpeed(moveSpeed);
			if (ImGui::DragFloat("回転感度 (マウス右)##fly", &sensitivity, 0.0001f, 0.0001f, 0.05f, "%.4f")) debugFlyCamera_->SetMouseSensitivity(sensitivity);
			if (ImGui::DragFloat("スクロール速度##fly", &scrollSpeed, 0.1f, 0.1f, 20.0f)) debugFlyCamera_->SetScrollSpeed(scrollSpeed);
			if (ImGui::DragFloat("パン速度 (中ボタン)##fly", &panSpeed, 0.001f, 0.001f, 1.0f)) debugFlyCamera_->SetPanSpeed(panSpeed);
			if (ImGui::DragFloat("回転速度 (キーボード)##fly", &rotateSpeed, 0.001f, 0.001f, 0.5f)) debugFlyCamera_->SetRotateSpeed(rotateSpeed);

			Vector3 flyPos = debugFlyCamera_->GetTranslate();
			float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
			if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) {
				debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
			}
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[プレイヤー視点 アクティブ]");
			if (ImGui::Button("フリーカメラに切り替え")) {
				SetDebugCameraActive(true);
			}
			if (ImGui::Checkbox("Cinematic lock-on camera", &isCinematicLockOnCameraEnabled_)) {
				isCinematicLockOnCameraInitialized_ = false;
			}
			ImGui::Text("Cinematic: %s", (isCinematicLockOnCameraEnabled_ && lockedEnemy_) ? "ACTIVE" : "OFF");
		}
	}

	ImGui::End();
#endif
}


void GamePlayScene::UpdateUI() {
#ifdef ENABLE_IMGUI
	if (ImGuiManager::IsVisible()) {
		if (IsSimulationMode()) {
			DrawSimulationScreenUI();
			return;
		}

		ImGui::Begin("Simulation");
		ImGui::Text("シミュレーション設定");
		ImGui::TextWrapped("保存済み設定の読み込みはここで行えます。細かい保存や確認は専用画面を開いてください。");
		if (ImGui::Button("シミュレーション画面を開く (F2)")) {
			LaunchSimulationExecutable();
		}
		DrawGameplayActionControls();
		ImGui::End();
		if (false) {

		// --- シミュレーション起動用ミニウィンドウ ---
		ImGui::Begin("シミュレーション", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
		if (ImGui::Button(showSimulationWindow_ ? "シミュレーションツールを閉じる" : "シミュレーションツールを開く")) {
			showSimulationWindow_ = !showSimulationWindow_;
		}
		ImGui::End();

		// --- シミュレーション詳細ウィンドウ ---
		if (showSimulationWindow_) {
			ImGui::SetNextWindowSize(ImVec2(600.0f, 500.0f), ImGuiCond_Once);
			ImGui::Begin("シミュレーションツール", &showSimulationWindow_);
			
			const char* categories[] = { "プレイヤー", "ミサイル", "敵 & イベント", "パーティクル", "カメラ" };
			ImGui::Combo("カテゴリ", &currentSimulationTarget_, categories, IM_ARRAYSIZE(categories));
			ImGui::Separator();

			if (currentSimulationTarget_ == 0) {
				ImGui::Text("プレイヤー移動設定");
				if (player_) {
					auto mode = player_->GetCurrentMode();
					const char* modeName = (mode == PlayerMode::Fighter) ? "ファイター (1キー)" : 
										   (mode == PlayerMode::Gerwalk) ? "ガウォーク (2キー)" : "バトロイド (3キー)";
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "現在の形態: %s", modeName);
					
					PlayerModeParams& p = player_->GetModeParams(mode);
					ImGui::SliderFloat("最大移動速度", &p.maxMoveSpeed, 0.01f, 1.0f);
					ImGui::SliderFloat("移動加速度", &p.moveAcceleration, 0.001f, 0.1f);
					ImGui::SliderFloat("移動減衰", &p.moveDamping, 0.1f, 1.0f);
					ImGui::SliderFloat("ピッチ回転速度", &p.pitchSpeed, 0.001f, 0.1f);
					ImGui::SliderFloat("ヨー回転速度", &p.yawSpeed, 0.001f, 0.1f);
					ImGui::SliderFloat("ロール回転速度", &p.rollSpeed, 0.001f, 0.1f);
				} else {
					ImGui::Text("プレイヤーが初期化されていません。");
				}
			}
			else if (currentSimulationTarget_ == 1) {
				DrawMissileSettingsUI();
			}
			else if (currentSimulationTarget_ == 2) {
				ImGui::Text("=== 敵の出現とルート ===");
				ImGui::Text("Lock-on: %s", lockedEnemy_ ? "LOCKED" : "NONE");
				ImGui::Text("Tab: lock target / X: unlock");
				ImGui::DragFloat3("出現座標 (X,Y,Z)", newEnemyPos, 1.0f);

				if (ImGui::Button("敵を生成する！")) {
					auto newEnemy = std::make_unique<Enemy>();
					newEnemy->Initialize({ newEnemyPos[0], newEnemyPos[1], newEnemyPos[2] });
					enemies_.push_back(std::move(newEnemy));
				}

				ImGui::Separator();
				ImGui::Text("=== 敵のリスト (総数: %d) ===", (int)enemies_.size());
				int index = 0;
				for (const auto& enemy : enemies_) {
					Vector3 pos = enemy->GetPosition();
					ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
					index++;
				}

				ImGui::Separator();
				ImGui::Text("イベントツール（増援設定）");
				static int selectedTrigger = 0;
				static int selectedTarget = 0;
				static int eventDelay = 60;
				std::vector<const char*> enemyNames;
				for (const auto& sp : enemySpawns_) {
					enemyNames.push_back(sp.name.c_str());
				}
				if (!enemyNames.empty()) {
					if (selectedTrigger >= enemyNames.size()) selectedTrigger = 0;
					if (selectedTarget >= enemyNames.size()) selectedTarget = 0;
					ImGui::Combo("トリガーとなる敵", &selectedTrigger, enemyNames.data(), static_cast<int>(enemyNames.size()));
					ImGui::Combo("出現する敵(ターゲット)", &selectedTarget, enemyNames.data(), static_cast<int>(enemyNames.size()));
					ImGui::DragInt("出現までのディレイ(フレーム)", &eventDelay, 1, 0, 600);
					if (ImGui::Button("イベントを追加")) {
						enemyEventManager_.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
						enemySpawns_[selectedTarget].isInitialSpawn = false;
					}
					ImGui::SameLine();
					if (ImGui::Button("イベントを保存")) {
						enemyEventManager_.SaveEvents("resources/enemy_events.json");
					}
					for (size_t i = 0; i < enemyEventManager_.GetEvents().size(); ++i) {
						const auto& ev = enemyEventManager_.GetEvents()[i];
						ImGui::Text("[%d] %s が死んだら %d F後に %s が出現", (int)i, ev.triggerEnemyName.c_str(), ev.delayFrames, ev.targetEnemyName.c_str());
						ImGui::SameLine();
						if (ImGui::Button(("削除##" + std::to_string(i)).c_str())) {
							enemyEventManager_.RemoveEvent(i);
							break; 
						}
					}
				}

				ImGui::Separator();
				ImGui::Text("敵機ルート確認 (Editor Preview)");
				if (ImGui::Button("リセット")) ResetEditorPreview();
				ImGui::SameLine();
				if (ImGui::Button("再生")) isEditorPreviewPlaying_ = true;
				ImGui::SameLine();
				if (ImGui::Button("ストップ")) isEditorPreviewPlaying_ = false;
				ImGui::TextColored(isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "状態: %s", isEditorPreviewPlaying_ ? "再生中" : "停止中");
			}
			else if (currentSimulationTarget_ == 3) {
				ImGui::Text("=== GPU Particles ===");
				bool gpuChanged = false;
				if (auto *emitter = particleManager->GetEmitterSphere()) {
					if (ImGui::DragFloat3("位置", &emitter->translate.x, 0.01f)) gpuChanged = true;
					if (ImGui::DragFloat("射出半径", &emitter->radius, 0.01f)) gpuChanged = true;
					if (ImGui::DragInt("射出数", (int *) &emitter->count, 1, 0, 1000)) gpuChanged = true;
					if (ImGui::DragFloat("射出間隔", &emitter->frequency, 0.01f, 0.01f, 10.0f)) gpuChanged = true;
				}
				if (ImGui::Button("GPUパーティクルを再初期化") || gpuChanged) {
					particleManager->RequestGpuInitialize();
				}
				
				ImGui::Separator();
				ImGui::Text("=== Explosion Particles ===");
				if (explosionManager_) {
					auto& config = explosionManager_->GetConfig();
					ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
					ImGui::ColorEdit4("カラー", config.color);
					ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f);
					ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f);
					ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f);
					ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f);
					ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f);
					ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f);
					ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f);
					if (ImGui::Button("設定をJSONに保存")) explosionManager_->SaveToJson("resources/explosionConfig.json");
					ImGui::SameLine();
					if (ImGui::Button("設定をJSONから読込")) explosionManager_->LoadFromJson("resources/explosionConfig.json");
				}
			}
			else if (currentSimulationTarget_ == 4) {
				ImGui::Text("=== Camera Settings ===");
				if (isDebugCameraActive_) {
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[FREE CAM ACTIVE]");
					if (ImGui::Button("Switch to Player Camera")) SetDebugCameraActive(false);
					float moveSpd = debugFlyCamera_->GetMoveSpeed();
					float rotSpd  = debugFlyCamera_->GetRotateSpeed();
					float sens    = debugFlyCamera_->GetMouseSensitivity();
					float scroll  = debugFlyCamera_->GetScrollSpeed();
					float pan     = debugFlyCamera_->GetPanSpeed();
					if (ImGui::DragFloat("移動速度 (WASD)##fly", &moveSpd, 0.01f, 0.01f, 20.0f)) debugFlyCamera_->SetMoveSpeed(moveSpd);
					if (ImGui::DragFloat("回転感度 (マウス右)##fly", &sens, 0.0001f, 0.0001f, 0.05f, "%.4f")) debugFlyCamera_->SetMouseSensitivity(sens);
					if (ImGui::DragFloat("スクロール速度##fly", &scroll, 0.1f, 0.1f, 20.0f)) debugFlyCamera_->SetScrollSpeed(scroll);
					if (ImGui::DragFloat("パン速度 (中ボタン)##fly", &pan, 0.001f, 0.001f, 1.0f)) debugFlyCamera_->SetPanSpeed(pan);
					if (ImGui::DragFloat("回転速度 (キーボード)##fly",&rotSpd, 0.001f, 0.001f, 0.5f)) debugFlyCamera_->SetRotateSpeed(rotSpd);
					Vector3 flyPos = debugFlyCamera_->GetTranslate();
					float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
					if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
				} else {
					ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[PLAYER FOLLOW CAM]");
					if (ImGui::Button("Switch to Debug Camera")) SetDebugCameraActive(true);
					if (ImGui::Checkbox("Cinematic lock-on camera", &isCinematicLockOnCameraEnabled_)) isCinematicLockOnCameraInitialized_ = false;
					ImGui::Text("Cinematic: %s", (isCinematicLockOnCameraEnabled_ && lockedEnemy_) ? "ACTIVE" : "OFF");
					Vector3 camPos = camera->GetTranslate();
					float camPosArr[3] = { camPos.x, camPos.y, camPos.z };
					if (ImGui::DragFloat3("カメラ位置 (参考)##follow", camPosArr, 0.1f)) camera->SetTranslate({ camPosArr[0], camPosArr[1], camPosArr[2] });
				}
			}

			ImGui::End();
		}

		// --- オリジナルUI（今まで出していたImGui関連） ---
		//開発用UIの処理
		}
		ImGui::ShowDemoWindow();
		//ウィンドウのサイズを設定
		ImGui::SetNextWindowSize(ImVec2(500.0f, 400.0f), ImGuiCond_Once);

		//ウィンドウの作成
		ImGui::Begin("演習");

		ImGui::Text("表示設定");
		ImGui::Checkbox("スカイボックスを表示", &showSkybox);
		ImGui::Checkbox("平面を表示", &showPlane);
		ImGui::Checkbox("球体（ミサイル頭）を表示", &showSphere);
		ImGui::Checkbox("通常リングを表示", &showNormalRing);
		ImGui::Checkbox("部分リングを表示", &showPartialRing);
		ImGui::Checkbox("シリンダーを表示", &showCylinder);
		ImGui::Checkbox("トレイルを表示", &showTrail);
		ImGui::Checkbox("モデルを表示", &showModel);
		ImGui::Checkbox("パーティクルを表示", &showParticles);
		ImGui::Checkbox("ボーンを表示", &showBones);
		ImGui::Checkbox("スプライトを表示", &showSprite);
		ImGui::Checkbox("Show Debug Colliders", &showDebugColliders);

		ImGui::Separator();
		ImGui::Text("デバック　カメラ");
		if (ImGui::Button(isDebugCameraActive_ ? "Switch to Player Camera" : "Switch to Debug Camera")) {
			SetDebugCameraActive(!isDebugCameraActive_);
		}
		ImGui::SameLine();
		ImGui::Text("%s", isDebugCameraActive_ ? "Active: Debug Camera" : "Active: Player Camera");

		ImGui::Separator();
		ImGui::Text("GPUパーティクルの操作");
		bool gpuChanged = false;
		if (auto *emitter = particleManager->GetEmitterSphere()) {
			if (ImGui::DragFloat3("位置", &emitter->translate.x, 0.01f)) gpuChanged = true;
			if (ImGui::DragFloat("射出半径", &emitter->radius, 0.01f)) gpuChanged = true;
			if (ImGui::DragInt("射出数", (int *) &emitter->count, 1, 0, 1000)) gpuChanged = true;
			if (ImGui::DragFloat("射出間隔", &emitter->frequency, 0.01f, 0.01f, 10.0f)) gpuChanged = true;
		}

		if (ImGui::Button("GPUパーティクルを再初期化") || gpuChanged) {
			particleManager->RequestGpuInitialize();
		}

		ImGui::Separator();
		ImGui::Text("アニメーション制御");
		const char *animationNames[] = { "AnimatedCube", "simpleSkin", "sneakWalk", "walk" };
		if (ImGui::Combo("アニメーション", &currentAnimationIndex, animationNames, IM_ARRAYSIZE(animationNames))) {
			std::string dir, file, loadFile;
			if (currentAnimationIndex == 0) { dir = "resources/AnimatedCube"; file = "AnimatedCube.gltf"; loadFile = "AnimatedCube/AnimatedCube.gltf"; } else if (currentAnimationIndex == 1) { dir = "resources/simpleSkin"; file = "simpleSkin.gltf"; loadFile = "simpleSkin/simpleSkin.gltf"; } else if (currentAnimationIndex == 2) { dir = "resources/human"; file = "sneakWalk.gltf"; loadFile = "human/sneakWalk.gltf"; } else if (currentAnimationIndex == 3) { dir = "resources/human"; file = "walk.gltf"; loadFile = "human/walk.gltf"; }

			animationData = LoadAnimationFile(dir, file);
			Node rootNode = Model::LoadNodeHierarchy(dir, file);
			skeleton = CreateSkeleton(rootNode);
			if (!skeleton.joints.empty()) {
				skeleton.joints[skeleton.root].transform.translate = { 0.0f, 0.0f, 0.0f };
			}
			animationTime = 0.0f;

			ModelManager::GetInstance()->LoadModel(loadFile);
			myModelObject->SetModel(loadFile);
			if (myModelObject->GetModel()) {
				myModelObject->skinCluster = myModelObject->GetModel()->CreateSkinCluster(skeleton);
			}
			showModel = true;
			enableSkinning = true;

			// モデルに応じた適切なスケールを自動設定する
			if (currentAnimationIndex == 0) {
				modelScale = 1.0f;
			} else {
				modelScale = 1.0f;  // simpleSkinなどは等倍で人間サイズ
			}
		}
		ImGui::Checkbox("スキニング (ガワを動かす)", &enableSkinning);
		ImGui::SliderFloat("モデルスケール", &modelScale, 0.001f, 1.0f);
		if (ImGui::Checkbox("アニメーション再生", &playAnimation)) {}
		ImGui::SliderFloat("再生時間", &animationTime, 0.0f, animationData.duration);

		ImGui::Separator();
		ImGui::Text("シリンダー設定");
		ImGui::DragFloat3("シリンダー座標", cylinderPos, 0.01f);
		ImGui::DragFloat3("シリンダースケール", cylinderScale, 0.01f);
		ImGui::DragFloat2("UVスクロール速度", cylinderUVScrollSpeed, 0.001f);
		ImGui::SliderFloat("アルファリファレンス", &cylinderAlphaReference, 0.0f, 1.0f);

		ImGui::Separator();
		DrawMissileSettingsUI();

		bool cChanged = false;
		if (ImGui::SliderInt("Subdivision##Cyl", &cylinderSubdivision, 3, 128)) cChanged = true;
		if (ImGui::SliderInt("Vertical Subdivision", &cylinderVerticalSubdivision, 1, 32)) cChanged = true;
		if (ImGui::DragFloat("Top Radius X", &cylinderTopRadiusX, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Top Radius Z", &cylinderTopRadiusZ, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Bottom Radius X", &cylinderBottomRadiusX, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Bottom Radius Z", &cylinderBottomRadiusZ, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Height", &cylinderHeight, 0.01f)) cChanged = true;
		if (ImGui::ColorEdit4("Top Color", cylinderTopColor)) cChanged = true;
		if (ImGui::ColorEdit4("Bottom Color", cylinderBottomColor)) cChanged = true;
		if (ImGui::SliderFloat("Start Angle##Cyl", &cylinderStartAngle, 0.0f, 360.0f)) cChanged = true;
		if (ImGui::SliderFloat("End Angle##Cyl", &cylinderEndAngle, 0.0f, 360.0f)) cChanged = true;
		if (ImGui::Checkbox("Flip UV", &cylinderIsUvFlipped)) cChanged = true;

		if (myCylinder && cChanged) {
			Model *model = myCylinder->GetModel();
			if (model) {
				model->InitializeCylinder(model->GetModelCommon(),
					cylinderSubdivision, cylinderVerticalSubdivision,
					cylinderTopRadiusX, cylinderTopRadiusZ,
					cylinderBottomRadiusX, cylinderBottomRadiusZ,
					cylinderHeight,
					{ cylinderTopColor[0], cylinderTopColor[1], cylinderTopColor[2], cylinderTopColor[3] },
					{ cylinderBottomColor[0], cylinderBottomColor[1], cylinderBottomColor[2], cylinderBottomColor[3] },
					cylinderStartAngle, cylinderEndAngle,
					cylinderIsUvFlipped);
			}
		}

		ImGui::Separator();
		ImGui::Text("Partial Ring Settings");

		bool pRingChanged = false;
		if (ImGui::SliderInt("Subdivision", &prSubdivision, 3, 128)) pRingChanged = true;
		if (ImGui::SliderFloat("Outer Radius", &prOuterRadius, 0.1f, 5.0f)) pRingChanged = true;
		if (ImGui::SliderFloat("Inner Radius", &prInnerRadius, 0.1f, 5.0f)) pRingChanged = true;
		if (ImGui::Checkbox("UV Horizontal", &prIsUvHorizontal)) pRingChanged = true;
		if (ImGui::ColorEdit4("Inner Color", prInnerColor)) pRingChanged = true;
		if (ImGui::ColorEdit4("Outer Color", prOuterColor)) pRingChanged = true;
		if (ImGui::SliderFloat("Start Angle", &prStartAngle, 0.0f, 360.0f)) pRingChanged = true;
		if (ImGui::SliderFloat("End Angle", &prEndAngle, 0.0f, 360.0f)) pRingChanged = true;
		if (ImGui::SliderFloat("Fade Angle", &prFadeAngle, 0.0f, 180.0f)) pRingChanged = true;

		if (pRingChanged && myPartialRing) {
			Model *model = myPartialRing->GetModel();
			if (model) {
				model->InitializeRing(
					model->GetModelCommon(), prSubdivision, prOuterRadius, prInnerRadius,
					prIsUvHorizontal, { prInnerColor[0], prInnerColor[1], prInnerColor[2], prInnerColor[3] },
					{ prOuterColor[0], prOuterColor[1], prOuterColor[2], prOuterColor[3] },
					prStartAngle, prEndAngle, prFadeAngle
				);
			}
		}

		//スプライトの座標を配列に格納
		Vector2 currentPos = sprite->GetPosition();
		float pos[2] = { currentPos.x,currentPos.y };

		//スライダーでスプライトの座標を変更できるようにする
		ImGui::SliderFloat2("Sprite Position", pos, 0.0f, 1280.0f, "%.1f");

		//スライダーで変更された座標をスプライトに反映
		sprite->SetPosition({ pos[0],pos[1] });

		ImGui::Separator();
		ImGui::Text("イベントツール（増援設定）");
        
		static int selectedTrigger = 0;
		static int selectedTarget = 0;
		static int eventDelay = 60;

		std::vector<const char*> enemyNames;
		for (const auto& sp : enemySpawns_) {
			enemyNames.push_back(sp.name.c_str());
		}

		if (!enemyNames.empty()) {
			if (selectedTrigger >= enemyNames.size()) selectedTrigger = 0;
			if (selectedTarget >= enemyNames.size()) selectedTarget = 0;

			ImGui::Combo("トリガーとなる敵", &selectedTrigger, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::Combo("出現する敵(ターゲット)", &selectedTarget, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::DragInt("出現までのディレイ(フレーム)", &eventDelay, 1, 0, 600);

			if (ImGui::Button("イベントを追加")) {
				enemyEventManager_.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
				enemySpawns_[selectedTarget].isInitialSpawn = false;
			}

			ImGui::SameLine();
			if (ImGui::Button("イベントを保存")) {
				enemyEventManager_.SaveEvents("resources/enemy_events.json");
			}

			ImGui::Separator();
			ImGui::Text("設定済みのイベント一覧");
			for (size_t i = 0; i < enemyEventManager_.GetEvents().size(); ++i) {
				const auto& ev = enemyEventManager_.GetEvents()[i];
				ImGui::Text("[%d] %s が死んだら %d F後に %s が出現", (int)i, ev.triggerEnemyName.c_str(), ev.delayFrames, ev.targetEnemyName.c_str());
				ImGui::SameLine();
				if (ImGui::Button(("削除##" + std::to_string(i)).c_str())) {
					enemyEventManager_.RemoveEvent(i);
					break; 
				}
			}
		}

		ImGui::End();

		ImGui::Begin("Camera Settings");

		// =====================================================
		if (isDebugCameraActive_) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[FREE CAM ACTIVE]");
			ImGui::Text("WASD: 移動  /  矢印キー: 回転  /  Q,E: ロール");
			ImGui::Text("Game View上で右ドラッグやWASDを使って確認できます");

			if (ImGui::Button("自機追従カメラに戻る")) {
				SetDebugCameraActive(false);
			}

			ImGui::Separator();
			ImGui::Text("操作方法:");
			ImGui::BulletText("右クリック + ドラッグ : 視点回転");
			ImGui::BulletText("スクロールホイール   : 前後移動");
			ImGui::BulletText("中クリック + ドラッグ : 上下左右パン");
			ImGui::BulletText("WASD                  : 前後左右移動");
			ImGui::BulletText("矢印キー              : 視点回転(キーボード)");
			ImGui::BulletText("Q / E                 : ロール");

			ImGui::Separator();
			// フリーカメラの速度調整（カメラから現在値を読む）
			float moveSpd = debugFlyCamera_->GetMoveSpeed();
			float rotSpd  = debugFlyCamera_->GetRotateSpeed();
			float sens    = debugFlyCamera_->GetMouseSensitivity();
			float scroll  = debugFlyCamera_->GetScrollSpeed();
			float pan     = debugFlyCamera_->GetPanSpeed();
			if (ImGui::DragFloat("移動速度 (WASD)##fly",      &moveSpd, 0.01f, 0.01f, 20.0f)) {
				debugFlyCamera_->SetMoveSpeed(moveSpd);
			}
			if (ImGui::DragFloat("回転感度 (マウス右)##fly",  &sens,    0.0001f, 0.0001f, 0.05f, "%.4f")) {
				debugFlyCamera_->SetMouseSensitivity(sens);
			}
			if (ImGui::DragFloat("スクロール速度##fly",       &scroll,  0.1f, 0.1f, 20.0f)) {
				debugFlyCamera_->SetScrollSpeed(scroll);
			}
			if (ImGui::DragFloat("パン速度 (中ボタン)##fly",  &pan,     0.001f, 0.001f, 1.0f)) {
				debugFlyCamera_->SetPanSpeed(pan);
			}
			if (ImGui::DragFloat("回転速度 (キーボード)##fly",&rotSpd,  0.001f, 0.001f, 0.5f)) {
				debugFlyCamera_->SetRotateSpeed(rotSpd);
			}

			ImGui::Separator();
			// フリーカメラの現在位置表示
			Vector3 flyPos = debugFlyCamera_->GetTranslate();
			float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
			if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) {
				debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
			}

		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[PLAYER FOLLOW CAM]");
			ImGui::Text("ボタンを押すとフリーカメラに切り替わります");

			if (ImGui::Button("フリーカメラに切り替え")) {
				SetDebugCameraActive(true);
			}

			ImGui::Separator();
			// 自機追従カメラの現在位置表示（参考用）
			if (ImGui::Checkbox("Cinematic lock-on camera", &isCinematicLockOnCameraEnabled_)) {
				isCinematicLockOnCameraInitialized_ = false;
			}
			ImGui::Text("Cinematic: %s", (isCinematicLockOnCameraEnabled_ && lockedEnemy_) ? "ACTIVE" : "OFF");
			ImGui::Separator();

			Vector3 camPos = camera->GetTranslate();
			float camPosArr[3] = { camPos.x, camPos.y, camPos.z };
			if (ImGui::DragFloat3("カメラ位置 (参考)##follow", camPosArr, 0.1f)) {
				camera->SetTranslate({ camPosArr[0], camPosArr[1], camPosArr[2] });
			}
		}

		ImGui::End();

		ImGui::Separator();

		ImGui::Begin("敵 & 障害物");
		ImGui::Text("=== ターゲット配置 ===");
		ImGui::Text("Lock-on: %s", lockedEnemy_ ? "LOCKED" : "NONE");
		ImGui::Text("Tab: lock target / X: unlock");
		ImGui::DragFloat3("出現座標 (X,Y,Z)", newEnemyPos, 1.0f);

		// ボタンを押した瞬間に、新しい敵をリストに追加！
		if (ImGui::Button("敵を生成する！")) {
			auto newEnemy = std::make_unique<Enemy>();
			newEnemy->Initialize({ newEnemyPos[0], newEnemyPos[1], newEnemyPos[2] });
			enemies_.push_back(std::move(newEnemy));
		}

		ImGui::Separator();
		ImGui::Text("=== 敵のリスト (総数: %d) ===", (int)enemies_.size());
		int index = 0;
		for (const auto& enemy : enemies_) {
			Vector3 pos = enemy->GetPosition();
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
			index++;
		}
		if (enemies_.empty()) {
			ImGui::Text("現在、敵は存在しません。");
		}

		ImGui::Separator();
		ImGui::Text("=== 障害物のリスト (総数: %d) ===", (int)obstacles_.size());
		
		if (ImGui::Button("障害物の設定をJSONに保存")) {
			SaveCurrentSimulationLayoutToSceneJson("resources/scene.json");
		}
		if (!simulationSaveMessage_.empty()) {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", simulationSaveMessage_.c_str());
		}

		int obsIndex = 0;
		for (const auto& obstacle : obstacles_) {
			Vector3 pos = obstacle->GetPosition();
			Vector3 colOff = obstacle->GetCollisionOffset();
			Vector3 colScale = obstacle->GetCollisionScale();
			bool colEnabled = obstacle->IsCollisionEnabled();
			bool useMeshCol = obstacle->IsUseMeshCollider();

			ImGui::PushID(obsIndex);
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", obsIndex, pos.x, pos.y, pos.z);
			
			if (ImGui::Checkbox("Collision Enabled", &colEnabled)) {
				obstacle->SetCollisionEnabled(colEnabled);
			}
			if (ImGui::Checkbox("Use Mesh Collider", &useMeshCol)) {
				obstacle->SetUseMeshCollider(useMeshCol);
			}
			if (ImGui::DragFloat3("Collision Offset", &colOff.x, 0.1f)) {
				obstacle->SetCollisionOffset(colOff);
			}
			if (ImGui::DragFloat3("Collision Scale", &colScale.x, 0.05f)) {
				obstacle->SetCollisionScale(colScale);
			}
			ImGui::PopID();
			
			obsIndex++;
		}
		if (obstacles_.empty()) {
			ImGui::Text("現在、障害物は存在しません。");
		}
		ImGui::End();

		ImGui::Begin("敵撃破パーティクル設定");
		if (explosionManager_) {
			auto& config = explosionManager_->GetConfig();
			ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
			ImGui::ColorEdit4("カラー", config.color);
			ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f);
			ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f);
			ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f);

			ImGui::Separator();
			if (ImGui::Button("設定をJSONに保存")) {
				explosionManager_->SaveToJson("resources/explosionConfig.json");
			}
			ImGui::SameLine();
			if (ImGui::Button("設定をJSONから読込")) {
				explosionManager_->LoadFromJson("resources/explosionConfig.json");
			}
		}
		ImGui::End();

#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
		const StageValidation::Report &stageValidationReport = StageValidation::GetLastReport();
		if (stageValidationReport.HasMessages() || stageValidationReport.HasCheckItems()) {
			if (gShowStageValidationWindow) {
				DrawStageValidationWindow(stageValidationReport);
			}

			if (gShowStageValidationLabels) {
				Camera *validationCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
				if (validationCamera) {
					DrawStageValidationOverlay(stageValidationReport, validationCamera->GetViewProjectionMatrix());
				}
			}
		}
#endif

		ImGui::Begin("Level Editor Tools"); // 新しいウィンドウを作る場合

#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
		ImGui::Text("レベル検査表示");
		ImGui::Checkbox("検査一覧を表示", &gShowStageValidationWindow);
		ImGui::Checkbox("警告ラベルを表示", &gShowStageValidationLabels);
		ImGui::Separator();
#endif

		// もしボタンが押されたら { } の中が実行される
		if (ImGui::Button("Open Blender")) {
			// ここでBlenderを起動！
			ShellExecuteA(nullptr, "open", "resources\\stage.blend", nullptr, nullptr, SW_SHOW);
		}

		ImGui::Separator();
		ImGui::Text("敵機ルート確認");
		if (ImGui::Button("リセット")) {
			ResetEditorPreview();
		}
		ImGui::SameLine();
		if (ImGui::Button("再生")) {
			isEditorPreviewPlaying_ = true;
			OutputDebugStringA("[EditorPreview] Play.\n");
		}
		ImGui::SameLine();
		if (ImGui::Button("ストップ")) {
			isEditorPreviewPlaying_ = false;
			OutputDebugStringA("[EditorPreview] Stop.\n");
		}

		ImGui::TextColored(
			isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
			"状態: %s",
			isEditorPreviewPlaying_ ? "再生中" : "停止中");

		ImGui::TextWrapped("リセットでscene.jsonを読み直して初期状態に戻し、停止状態にします。再生で敵機ルートなどのゲーム更新が進み、ストップでその場に止まります。");

		ImGui::End();
	}
#endif
}
