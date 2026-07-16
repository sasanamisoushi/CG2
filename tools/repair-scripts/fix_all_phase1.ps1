param()
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

# 1. Fix GamePlayScene.cpp
$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$textScene = [IO.File]::ReadAllText($sceneCpp, $utf8NoBom)
# Remove lingering myRing initialization
$textScene = [regex]::Replace($textScene, '(?m)^\s*myRing = std::make_unique<Primitive>\(\);\r?\n^\s*myRing->Initialize.*?;\r?\n^\s*myRing->SetTranslate.*?;\r?\n', '')
# Fix showParticles
$textScene = [regex]::Replace($textScene, 'if \(showParticles', 'if (environmentRenderer_->GetShowParticles()')
[IO.File]::WriteAllText($sceneCpp, $textScene, $utf8NoBom)
Write-Host "GamePlayScene.cpp fixed"

# 2. Fix GamePlayUIManager.cpp
$uiCpp = "project\Game\Scene\GamePlayUIManager.cpp"
$textUI = [IO.File]::ReadAllText($uiCpp, $utf8NoBom)
$textUI = [regex]::Replace($textUI, '(?m)scene_->myRing\b', 'scene_->environmentRenderer_->myRing_')
$textUI = [regex]::Replace($textUI, '(?m)scene_->myCylinder\b', 'scene_->environmentRenderer_->myCylinder_')
$textUI = [regex]::Replace($textUI, '(?m)scene_->particleManager\b', 'scene_->environmentRenderer_->GetParticleManager()')
$textUI = [regex]::Replace($textUI, '(?m)scene_->particleEmitter\b', 'scene_->environmentRenderer_->GetParticleEmitter()')
$textUI = [regex]::Replace($textUI, '(?m)scene_->showSkybox\b', 'scene_->environmentRenderer_->showSkybox_')
$textUI = [regex]::Replace($textUI, '(?m)scene_->showParticles\b', 'scene_->environmentRenderer_->showParticles_')
[IO.File]::WriteAllText($uiCpp, $textUI, $utf8NoBom)
Write-Host "GamePlayUIManager.cpp fixed"

# 3. Fix SimulationManager.cpp
$simCpp = "project\Game\Scene\SimulationManager.cpp"
$textSim = [IO.File]::ReadAllText($simCpp, $utf8NoBom)
$textSim = [regex]::Replace($textSim, '(?m)scene_->particleManager', 'scene_->environmentRenderer_->GetParticleManager()')
[IO.File]::WriteAllText($simCpp, $textSim, $utf8NoBom)
Write-Host "SimulationManager.cpp fixed"

# 4. Fix EnvironmentRenderer.h to add showSkybox_ and showParticles_
$envH = "project\Game\Scene\EnvironmentRenderer.h"
$textEnvH = [IO.File]::ReadAllText($envH, $utf8NoBom)
if ($textEnvH -notmatch 'showSkybox_') {
    $textEnvH = $textEnvH.Replace('bool showCylinder_ = false;', "bool showCylinder_ = false;`n`tbool showSkybox_ = true;`n`tbool showParticles_ = true;")
    $textEnvH = $textEnvH.Replace('Skybox* GetSkybox() const { return skybox_.get(); }', "Skybox* GetSkybox() const { return skybox_.get(); }`n`tbool GetShowSkybox() const { return showSkybox_; }`n`tbool GetShowParticles() const { return showParticles_; }")
}
[IO.File]::WriteAllText($envH, $textEnvH, $utf8NoBom)
Write-Host "EnvironmentRenderer.h fixed"

# 5. Fix EnvironmentRenderer.cpp to use showSkybox_ and showParticles_
$envCpp = "project\Game\Scene\EnvironmentRenderer.cpp"
$textEnvCpp = [IO.File]::ReadAllText($envCpp, $utf8NoBom)
$textEnvCpp = [regex]::Replace($textEnvCpp, '(?m)^\s*if \(skybox_\) \{', 'if (skybox_ && showSkybox_) {')
$textEnvCpp = [regex]::Replace($textEnvCpp, '(?m)^\s*if \(particleManager_\) \{', 'if (particleManager_ && showParticles_) {')
[IO.File]::WriteAllText($envCpp, $textEnvCpp, $utf8NoBom)
Write-Host "EnvironmentRenderer.cpp fixed"

