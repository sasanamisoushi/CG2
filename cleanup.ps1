param()

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"

$lines = [IO.File]::ReadAllLines($sceneCpp, $utf8NoBom)

# We want to remove lines 39 to 235 (indices 38 to 234)
# And replace UpdateUI (lines 1595 to EOF, indices 1594 to EOF)
# Wait! Instead of hardcoding indices (which might shift if we insert things), we just find the indices!

$startVal = -1
$endVal = -1
for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($lines[$i] -match '^#if defined\(ENABLE_IMGUI\) && defined\(CG2_ENABLE_STAGE_VALIDATION\)') {
        if ($startVal -eq -1) { $startVal = $i }
    }
    if ($lines[$i] -match '^GamePlayScene::GamePlayScene') {
        # The #endif is right before this line
        $endVal = $i - 1
        # Skip empty lines backwards
        while ($lines[$endVal] -match '^\s*$') { $endVal-- }
        # endVal should now point to #endif
        break
    }
}

$startUI = -1
for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($lines[$i] -match '^void GamePlayScene::UpdateUI\(\) \{') {
        $startUI = $i
        break
    }
}

$newLines = [System.Collections.ArrayList]::new()
for ($i = 0; $i -lt $lines.Length; $i++) {
    # Skip validation block
    if ($i -ge $startVal -and $i -le $endVal) {
        continue
    }
    
    # Skip UpdateUI block
    if ($i -ge $startUI) {
        continue # this skips to the end of file
    }
    
    $newLines.Add($lines[$i])
    
    # Insert uiManager_ initialization right after camera init
    if ($lines[$i] -match 'camera = std::make_unique<Camera>\(\);') {
        $newLines.Add("	uiManager_ = std::make_unique<GamePlayUIManager>(this);")
    }
}

# Now add the new UpdateUI
$newLines.Add("void GamePlayScene::UpdateUI() {")
$newLines.Add("#ifdef ENABLE_IMGUI")
$newLines.Add("	if (uiManager_) {")
$newLines.Add("		uiManager_->UpdateUI();")
$newLines.Add("	}")
$newLines.Add("#endif")
$newLines.Add("}")

[IO.File]::WriteAllLines($sceneCpp, $newLines.ToArray(), $utf8NoBom)
Write-Host "GamePlayScene.cpp cleaned up properly!"
