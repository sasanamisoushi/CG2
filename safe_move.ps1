param()
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$lines = [IO.File]::ReadAllLines($sceneCpp, $utf8NoBom)
$newLines = @()

$inDraw = $false
$inUpdateUI = $false
$drawLines = @()

for ($i = 0; $i -lt $lines.Length; $i++) {
    $line = $lines[$i]
    
    if ($line.StartsWith("void GamePlayScene::DrawGameplayActionControls() {")) {
        $inDraw = $true
    }
    
    if ($line.StartsWith("void GamePlayScene::UpdateUI() {")) {
        $inUpdateUI = $true
    }
    
    if ($inDraw) {
        $drawLines += $line
        if ($line -eq "}") {
            $inDraw = $false
        }
        continue
    }
    
    if ($inUpdateUI) {
        if ($line -eq "}") {
            $inUpdateUI = $false
        }
        continue
    }
    
    # Check for UpdateUI call inside Update()
    if ($line.Trim() -eq "UpdateUI();") {
        # Check previous line is #ifdef ENABLE_IMGUI or something? Just replace.
        $newLines += "	if (uiManager_) {"
        $newLines += "		if (environmentRenderer_) environmentRenderer_->Update(camera.get());"
        $newLines += "		uiManager_->UpdateUI();"
        $newLines += "	}"
        continue
    }
    
    $newLines += $line
}

[IO.File]::WriteAllLines($sceneCpp, $newLines, $utf8NoBom)
Write-Host "GamePlayScene.cpp updated safely"

# Now append drawLines to GamePlayUIManager.cpp
$uiCpp = "project\Game\Scene\GamePlayUIManager.cpp"
$uiLines = [IO.File]::ReadAllLines($uiCpp, $utf8NoBom)
$uiNewLines = @()
$uiNewLines += $uiLines

$uiNewLines += ""
for ($i = 0; $i -lt $drawLines.Length; $i++) {
    $dl = $drawLines[$i]
    if ($dl.StartsWith("void GamePlayScene::DrawGameplayActionControls() {")) {
        $dl = "void GamePlayUIManager::DrawGameplayActionControls() {"
    }
    $dl = $dl.Replace("simulationManager_->", "scene_->simulationManager_->")
    $uiNewLines += $dl
}

[IO.File]::WriteAllLines($uiCpp, $uiNewLines, $utf8NoBom)
Write-Host "GamePlayUIManager.cpp updated safely"
