param()

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$sceneH = "project\Game\Scene\GamePlayScene.h"
$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$uiH = "project\Game\Scene\GamePlayUIManager.h"
$uiCpp = "project\Game\Scene\GamePlayUIManager.cpp"

$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

Write-Host "Reading files..."
$hText = [IO.File]::ReadAllText($sceneH, $utf8NoBom)
$cppText = [IO.File]::ReadAllText($sceneCpp, $utf8NoBom)
$uiHText = [IO.File]::ReadAllText($uiH, $utf8NoBom)

# 1. Update GamePlayUIManager.h
$newUiHText = @"
#pragma once

#include <string>
#include <vector>

class GamePlayScene;

// ゲームプレイUIおよびシミュレーションツールのUI描画を管理するクラス
class GamePlayUIManager {
public:
	GamePlayUIManager(GamePlayScene* scene);
	~GamePlayUIManager() = default;

	void Initialize();
	void UpdateUI();

private:
	GamePlayScene* scene_ = nullptr;
};
"@

[IO.File]::WriteAllText($uiH, $newUiHText, $utf8NoBom)

# 2. Extract UpdateUI from GamePlayScene.cpp
$match = [regex]::Match($cppText, '(?s)(void GamePlayScene::UpdateUI\(\) \{)(.*)')
if (-not $match.Success) {
    Write-Host "Could not find UpdateUI in GamePlayScene.cpp"
    exit 1
}

$updateUIBody = $match.Groups[2].Value

# Remove UpdateUI body from GamePlayScene.cpp and add the new one
$newCppText = $cppText.Substring(0, $match.Index) + "void GamePlayScene::UpdateUI() {`n#ifdef ENABLE_IMGUI`n	if (uiManager_) {`n		uiManager_->UpdateUI();`n	}`n#endif`n}`n"

# Also change uiManager_ = std::make_unique<GamePlayUIManager>(); to pass 'this'
$newCppText = $newCppText -replace 'uiManager_ = std::make_unique<GamePlayUIManager>\(\);', 'uiManager_ = std::make_unique<GamePlayUIManager>(this);'

[IO.File]::WriteAllText($sceneCpp, $newCppText, $utf8NoBom)

# 3. Create GamePlayUIManager.cpp with the extracted body
$vars = @(
    "mode_", "camera", "sprite", "groundModel", "myShere", "skybox", "aimCursorSprite_", "lockOnReticleSprite_", 
    "boundaryAlertObject_", "ceilingBoundaryAlertObject_", "particleManager", "particleEmitter", "soundData1", 
    "soundData2", "boundaryAlertPlane_", "myBox", "myRing", "myPartialRing", "myCylinder", "myModelObject", 
    "showNormalRing", "showPartialRing", "showCylinder", "prSubdivision", "prOuterRadius", "prInnerRadius", 
    "prIsUvHorizontal", "prInnerColor", "prOuterColor", "prStartAngle", "prEndAngle", "prFadeAngle", 
    "cylinderPos", "cylinderScale", "cylinderUVOffset", "cylinderUVScrollSpeed", "cylinderAlphaReference", 
    "cylinderSubdivision", "cylinderVerticalSubdivision", "cylinderTopRadiusX", "cylinderTopRadiusZ", 
    "cylinderBottomRadiusX", "cylinderBottomRadiusZ", "cylinderHeight", "cylinderTopColor", "cylinderBottomColor", 
    "cylinderStartAngle", "cylinderEndAngle", "cylinderIsUvFlipped", "animationData", "animationTime", "playAnimation", 
    "skeleton", "showBones", "showPlane", "showSphere", "showBox", "showTrail", "showSkybox", "showSprite", 
    "skeletonLinesModel", "skeletonLinesObject", "debugColliderLinesObject", "showDebugColliders", "debugFlyCamera_", 
    "isDebugCameraActive_", "isEditorPreviewPlaying_", "isCinematicLockOnCameraEnabled_", "isCinematicLockOnCameraInitialized_", 
    "cinematicLockOnCameraPosition_", "cinematicLockOnCameraRotation_", "cinematicLockOnCameraFocus_", 
    "cinematicLockOnCameraBackDirection_", "cinematicLockOnCameraSideSign_", "cinematicLockOnCameraSeparation_", 
    "showParticles", "showModel", "enableSkinning", "modelScale", "currentAnimationIndex", "missileTrail", 
    "trailObject", "missileNormalSpeed", "missileNormalScale", "missileNormalCollisionRadius", "missileNormalLifeTime", 
    "missileSpeed", "missileAmpX", "missileAmpZ", "missileAmpY", "missileFreqY", "missileBaseY", "missileHomingStrength", 
    "missileHomingScale", "missileHomingCollisionRadius", "missileTrailWidth", "missileLifeTime", "missileMuzzleOffset", 
    "player_", "missileManager_", "enemies_", "enemyBulletManager_", "enemySpawns_", "enemyRespawnTimers_", 
    "obstacles_", "newEnemyPos", "explosionManager_", "isGameOver_", "gameOverTimer_", "showSimulationWindow_", 
    "currentSimulationTarget_", "simulationSaveMessage_", "simulationActionName_", "simulationActionNames_", 
    "selectedSimulationActionIndex_", "simulationActionMessage_", "simulationPlaybackMode_", "missilePresetName_", 
    "missilePresetTypeIndex_", "missilePresetNames_", "selectedMissilePresetIndex_", "missilePresetMessage_", 
    "simulationManager_", "missilePresetManager_", "lockOnManager_", "cameraManager_", "levelManager_", 
    "environmentRenderer_", "uiManager_", "lastJsonWriteTime_", "aimAssistEnemy_", "isMultiLockCharging_", 
    "multiLockChargeFrames_"
)

$refLines = ""
foreach ($v in $vars) {
    # Using #define is a foolproof way to replace instances without worrying about shadowed locals or types!
    $refLines += "#define $v scene_->$v`n"
}

# The methods too!
$funcs = @(
    "IsSimulationMode", "DrawGameplayActionControls", "SetDebugCameraActive", "ReloadSceneJson", "ResetEditorPreview", 
    "MakeMissileTuning", "SpawnEnemyFromSpawnPoint", "IsEnemySpawnPointActive", "ScheduleEnemySpawn", 
    "TriggerEnemyReinforcements", "UpdateEnemyRespawns", "HasPendingEnemySpawns", "UpdateCinematicLockOnCamera", 
    "LaunchSimulationExecutable"
)

foreach ($f in $funcs) {
    $refLines += "#define $f scene_->$f`n"
}

# Un-define at the end
$undefLines = ""
foreach ($v in $vars) {
    $undefLines += "#undef $v`n"
}
foreach ($f in $funcs) {
    $undefLines += "#undef $f`n"
}

$newUiCppText = @"
#include "GamePlayUIManager.h"
#include "GamePlayScene.h"
#include "engine/Debug/ImGuiManager.h"
#include <externals/imgui/imgui.h>

GamePlayUIManager::GamePlayUIManager(GamePlayScene* scene) : scene_(scene) {
}

void GamePlayUIManager::Initialize() {
}

void GamePlayUIManager::UpdateUI() {
    if (!scene_) return;

$refLines
$updateUIBody
$undefLines
"@

# Fix any unclosed braces if needed, but updateUIBody already has the closing brace of GamePlayScene::UpdateUI()?
# Actually, $match.Groups[2].Value includes everything until EOF. Since it's the last method, it includes the closing brace of the function and trailing whitespace.
# Let's insert the undefLines before the very last closing brace.
$lastBraceIndex = $newUiCppText.LastIndexOf('}')
$newUiCppText = $newUiCppText.Substring(0, $lastBraceIndex) + "`n" + $undefLines + "`n}" + $newUiCppText.Substring($lastBraceIndex + 1)

[IO.File]::WriteAllText($uiCpp, $newUiCppText, $utf8NoBom)

# 4. Update GamePlayScene.h
if (-not ($hText -match 'friend class GamePlayUIManager;')) {
    $hText = $hText -replace 'class GamePlayScene :public BaseScene \{', "class GamePlayScene :public BaseScene {`npublic:`n	friend class GamePlayUIManager;"
}
[IO.File]::WriteAllText($sceneH, $hText, $utf8NoBom)

Write-Host "Refactoring complete!"
