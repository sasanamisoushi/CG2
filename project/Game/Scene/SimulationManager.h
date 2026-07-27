#pragma once

#include <string>
#include <vector>
#include "engine/Camera/Camera.h"
#include "Game/enemy/Enemy.h"
#include "Game/Player/Player.h"
#include "Game/bullet/Missile.h"
#include "externals/json.hpp"

#include <set>

class GamePlayScene;

class SimulationManager {
public:
	SimulationManager(GamePlayScene* scene);
	bool SaveCurrentSimulationLayoutToSceneJson(const std::string &filePath);
	void RefreshSimulationActionNames();
	bool SaveNamedSimulationAction(const std::string &filePath, const std::string &actionName);
	bool ApplySimulationAction(const std::string &filePath, const std::string &actionName);
	void DrawSimulationScreenUI();
	void DrawSimulationSaveControls();

	void DrawAnimationEditorUI();
	void RefreshActionAnimationsList();

	const std::set<std::string>& GetSelectedBoneNames() const { return selectedBoneNames_; }
	void AddSelectedBoneName(const std::string& name) { selectedBoneNames_.insert(name); }
	void RemoveSelectedBoneName(const std::string& name) { selectedBoneNames_.erase(name); }
	void ClearSelectedBones() { selectedBoneNames_.clear(); }
	bool IsBoneSelected(const std::string& name) const { return selectedBoneNames_.count(name) > 0; }
	
	void AddBoneRotationFromDrag(const std::string& boneName, float deltaX, float deltaY);
	void AddBoneTranslationFromDrag(const std::string& boneName, float deltaX, float deltaY);
	
	void UpdateShortcuts();
	void PushUndo();
	void Undo();
	void Redo();
	void DeleteSelectedBonesKeyframes();

private:
	GamePlayScene* scene_;

	// アニメーション編集用状態
	Animation customAnimation_;
	char actionName_[128] = "Guard";
	std::vector<std::string> availableActionAnimations_;
	
	std::set<std::string> selectedBoneNames_;
	float currentFrameTime_ = 0.0f;
	bool isAnimationPlaying_ = false;
	bool isLooping_ = true;

	std::vector<Animation> undoHistory_;
	std::vector<Animation> redoHistory_;
	bool isDragging_ = false; // ドラッグ状態の追跡
};

