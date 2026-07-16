param()

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$uiCpp = "project\Game\Scene\GamePlayUIManager.cpp"
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

$cppText = [IO.File]::ReadAllText($sceneCpp, $utf8NoBom)
$uiCppText = [IO.File]::ReadAllText($uiCpp, $utf8NoBom)

# 1. Move Validation Block
$match = [regex]::Match($cppText, '(?s)(#if defined\(ENABLE_IMGUI\) && defined\(CG2_ENABLE_STAGE_VALIDATION\).*?#endif)(\s*GamePlayScene::GamePlayScene)')
if ($match.Success) {
    $validationBlock = $match.Groups[1].Value
    $cppText = $cppText.Substring(0, $match.Index) + $match.Groups[2].Value
    [IO.File]::WriteAllText($sceneCpp, $cppText, $utf8NoBom)
    
    $includes = @"
#include "SimulationManager.h"
#include "MissilePresetManager.h"
#include "LockOnManager.h"
#include "engine/Utility/StageValidation.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/math/MyMath.h"
#include "externals/json.hpp"
"@
    
    # Replace includes
    $uiCppText = $uiCppText -replace '#include <externals/imgui/imgui.h>', "#include <externals/imgui/imgui.h>`n$includes`n`n$validationBlock`n"
}

# 2. Add missing macros
$missingMacros = @"
#define enemyEventManager_ scene_->enemyEventManager_
#define lockedEnemy_ scene_->lockedEnemy_
#define aimAssistEnemy_ scene_->aimAssistEnemy_
#define isMultiLockCharging_ scene_->isMultiLockCharging_
#define multiLockTargets_ scene_->multiLockTargets_
"@

$missingUndefs = @"
#undef enemyEventManager_
#undef lockedEnemy_
#undef aimAssistEnemy_
#undef isMultiLockCharging_
#undef multiLockTargets_
"@

# Insert macros right after "if (!scene_) return;"
$uiCppText = $uiCppText -replace 'if \(\!scene_\) return;', "if (!scene_) return;`n$missingMacros"

# Insert undefs right before the last closing brace
$lastBrace = $uiCppText.LastIndexOf('}')
$uiCppText = $uiCppText.Substring(0, $lastBrace) + "`n" + $missingUndefs + "`n}"

[IO.File]::WriteAllText($uiCpp, $uiCppText, $utf8NoBom)

Write-Host "Fixes applied!"
