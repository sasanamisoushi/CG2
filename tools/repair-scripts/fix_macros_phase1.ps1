param()
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

# 1. Fix GamePlayUIManager.cpp
$uiCpp = "project\Game\Scene\GamePlayUIManager.cpp"
$textUI = [IO.File]::ReadAllText($uiCpp, $utf8NoBom)
$textUI = [regex]::Replace($textUI, '(?m)^\s*#define environmentRenderer_ scene_->environmentRenderer_\r?\n', '')
$textUI = [regex]::Replace($textUI, '(?m)^\s*#undef environmentRenderer_\r?\n', '')
$textUI = [regex]::Replace($textUI, '(?m)scene_->myPartialRing', 'scene_->environmentRenderer_->myPartialRing_')
[IO.File]::WriteAllText($uiCpp, $textUI, $utf8NoBom)
Write-Host "GamePlayUIManager.cpp fixed"

# 2. Fix GamePlayScene.cpp
$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$textScene = [IO.File]::ReadAllText($sceneCpp, $utf8NoBom)

$textScene = $textScene.Replace('particleManager->CreateParticleGroup("test", "resources/circle.png");', 'environmentRenderer_->GetParticleManager()->CreateParticleGroup("test", "resources/circle.png");')
$textScene = $textScene.Replace('explosionManager_->Initialize(particleManager.get());', 'explosionManager_->Initialize(environmentRenderer_->GetParticleManager());')

$textScene = [regex]::Replace($textScene, '(?s)\s*if \(showSkybox && skybox\) \{\s*skybox->Draw\(\);\s*\}', '')
$textScene = [regex]::Replace($textScene, '(?m)^\s*particleManager->Draw\(\);\r?\n', '')

[IO.File]::WriteAllText($sceneCpp, $textScene, $utf8NoBom)
Write-Host "GamePlayScene.cpp fixed"
