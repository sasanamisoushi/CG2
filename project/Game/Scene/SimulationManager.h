#pragma once

#include <string>
#include <vector>
#include "engine/Camera/Camera.h"
#include "Game/enemy/Enemy.h"
#include "Game/Player/Player.h"
#include "Game/bullet/Missile.h"
#include "externals/json.hpp"

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

private:
	GamePlayScene* scene_;
};
