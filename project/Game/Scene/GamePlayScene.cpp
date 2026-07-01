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
	// プリミティブ�E生�E�E�E
	//======================================================

	// 平面�E��EのチェチE��ーボ�Eド！E
	myPlane = std::make_unique<Primitive>();
	myPlane->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Plane);
	myPlane->SetTranslate({ 0.0f, 2.0f, 0.0f }); // 允E�E上�E方の配置
	objects.push_back(myPlane.get());

	// 琁E��E
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
// 		UpdateGameplayCamera();
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
// 		UpdateGameplayCamera();
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



void GamePlayScene::UpdateUI() {
#ifdef ENABLE_IMGUI
    if (uiManager_) {
        uiManager_->UpdateUI();
    }
#endif
}

