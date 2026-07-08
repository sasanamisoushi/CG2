param()
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$textScene = [IO.File]::ReadAllText($sceneCpp, $utf8NoBom)

# Extract the block
$pattern = '(?s)\nvoid GamePlayScene::DrawGameplayActionControls\(\) \{.*?\n\}\n'
$match = [regex]::Match($textScene, $pattern)
if ($match.Success) {
    $extracted = $match.Value
    # Remove from GamePlayScene.cpp
    $textScene = $textScene.Replace($extracted, "")
    
    # Change signature and simulationManager_
    $extracted = $extracted.Replace("void GamePlayScene::DrawGameplayActionControls()", "void GamePlayUIManager::DrawGameplayActionControls()")
    $extracted = $extracted.Replace("simulationManager_->", "scene_->simulationManager_->")
    
    # Append to GamePlayUIManager.cpp
    $uiCpp = "project\Game\Scene\GamePlayUIManager.cpp"
    $textUI = [IO.File]::ReadAllText($uiCpp, $utf8NoBom)
    $textUI += "`r`n$extracted"
    [IO.File]::WriteAllText($uiCpp, $textUI, $utf8NoBom)
    
    Write-Host "DrawGameplayActionControls moved"
} else {
    Write-Host "DrawGameplayActionControls not found"
}

# Remove UpdateUI() from GamePlayScene.cpp
$pattern2 = '(?s)\nvoid GamePlayScene::UpdateUI\(\) \{.*?\n\}\n'
$match2 = [regex]::Match($textScene, $pattern2)
if ($match2.Success) {
    $textScene = $textScene.Replace($match2.Value, "")
    Write-Host "UpdateUI removed"
}

# Fix Update() in GamePlayScene.cpp
$textScene = $textScene.Replace("UpdateUI();", "if (uiManager_) { if (environmentRenderer_) environmentRenderer_->Update(camera.get()); uiManager_->UpdateUI(); }")

[IO.File]::WriteAllText($sceneCpp, $textScene, $utf8NoBom)
Write-Host "GamePlayScene.cpp updated"
