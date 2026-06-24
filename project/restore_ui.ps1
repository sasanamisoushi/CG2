$restored = Get-Content -Path "c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp_restored" -Encoding UTF8
$funcs = @("void GamePlayScene::RefreshSimulationActionNames", "bool GamePlayScene::ApplySimulationAction", "void GamePlayScene::RefreshMissilePresetNames", "bool GamePlayScene::SaveMissilePreset", "bool GamePlayScene::ApplyMissilePreset", "void GamePlayScene::DrawSimulationSaveControls", "void GamePlayScene::DrawGameplayActionControls", "void GamePlayScene::DrawMissileSettingsUI", "void GamePlayScene::DrawSimulationScreenUI", "void GamePlayScene::UpdateUI")

$extracted = @()
$inFunc = $false
$braceCount = 0

foreach ($line in $restored) {
    if (-not $inFunc) {
        foreach ($f in $funcs) {
            if ($line -match "^$f") {
                $inFunc = $true
                $braceCount = 0
                break
            }
        }
    }
    
    if ($inFunc) {
        # Add u8 prefix to string literals containing non-ASCII characters as a fallback
        $modifiedLine = $line -replace '("[^"]*[^\x00-\x7F]+[^"]*")', 'u8$1'
        $modifiedLine = $modifiedLine -replace 'u8u8', 'u8'
        $extracted += $modifiedLine
        
        $braceCount += ($line.ToCharArray() | Where-Object { $_ -eq '{' }).Count
        $braceCount -= ($line.ToCharArray() | Where-Object { $_ -eq '}' }).Count
        
        if ($braceCount -le 0 -and $line -match '}') {
            $inFunc = $false
        }
    }
}

$current = Get-Content -Path "c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp" -Encoding UTF8
$filtered = $current | Where-Object {
    $line = $_
    $isDummy = $false
    foreach ($f in $funcs) {
        if ($line -match "^$f" -and ($line -match "\{\s*\}" -or $line -match "return false;\s*\}")) {
            $isDummy = $true
            break
        }
    }
    -not $isDummy
}

$finalContent = $filtered + $extracted
$utf8BOM = New-Object System.Text.UTF8Encoding $true
[System.IO.File]::WriteAllLines("c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp", $finalContent, $utf8BOM)
Write-Host ("Extracted " + $extracted.Length + " lines.")
