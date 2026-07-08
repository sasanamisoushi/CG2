param()

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

function ProcessHeader($filepath) {
    $lines = [IO.File]::ReadAllLines($filepath, $utf8NoBom)
    $newLines = [System.Collections.ArrayList]::new()
    $hasIncluded = $false
    
    foreach ($line in $lines) {
        if ($line -match '^\s*int\s+prSubdivision') { continue }
        if ($line -match '^\s*float\s+prOuterRadius') { continue }
        if ($line -match '^\s*float\s+prInnerRadius') { continue }
        if ($line -match '^\s*bool\s+prIsUvHorizontal') { continue }
        if ($line -match '^\s*float\s+prInnerColor') { continue }
        if ($line -match '^\s*float\s+prOuterColor') { continue }
        if ($line -match '^\s*float\s+prStartAngle') { continue }
        if ($line -match '^\s*float\s+prEndAngle') { continue }
        if ($line -match '^\s*float\s+prFadeAngle') { continue }
        
        if ($line -match '^\s*float\s+cylinderPos') { continue }
        if ($line -match '^\s*float\s+cylinderScale') { continue }
        if ($line -match '^\s*float\s+cylinderUVOffset') { continue }
        if ($line -match '^\s*float\s+cylinderUVScrollSpeed') { continue }
        if ($line -match '^\s*float\s+cylinderAlphaReference') { continue }
        if ($line -match '^\s*int\s+cylinderSubdivision') { continue }
        if ($line -match '^\s*int\s+cylinderVerticalSubdivision') { continue }
        if ($line -match '^\s*float\s+cylinderTopRadiusX') { continue }
        if ($line -match '^\s*float\s+cylinderTopRadiusZ') { continue }
        if ($line -match '^\s*float\s+cylinderBottomRadiusX') { continue }
        if ($line -match '^\s*float\s+cylinderBottomRadiusZ') { continue }
        if ($line -match '^\s*float\s+cylinderHeight') { continue }
        if ($line -match '^\s*float\s+cylinderTopColor') { continue }
        if ($line -match '^\s*float\s+cylinderBottomColor') { continue }
        if ($line -match '^\s*float\s+cylinderStartAngle') { continue }
        if ($line -match '^\s*float\s+cylinderEndAngle') { continue }
        if ($line -match '^\s*bool\s+cylinderIsUvFlipped') { continue }
        
        if ($line -match '^\s*bool\s+showNormalRing') { continue }
        if ($line -match '^\s*bool\s+showPartialRing') { continue }
        if ($line -match '^\s*bool\s+showCylinder') { continue }
        if ($line -match '^\s*bool\s+showSkybox') { continue }
        if ($line -match '^\s*bool\s+showParticles') { continue }
        
        if ($line -match 'std::unique_ptr<Skybox>\s+skybox') { continue }
        if ($line -match 'std::unique_ptr<ParticleManager>\s+particleManager') { continue }
        if ($line -match 'std::unique_ptr<ParticleEmitter>\s+particleEmitter') { continue }
        if ($line -match 'std::unique_ptr<Primitive>\s+boundaryAlertPlane_') { continue }
        if ($line -match 'std::unique_ptr<Primitive>\s+myRing') { continue }
        if ($line -match 'std::unique_ptr<Primitive>\s+myPartialRing') { continue }
        if ($line -match 'std::unique_ptr<Primitive>\s+myCylinder') { continue }
        
        if ($line -match '#include "EnvironmentRenderer.h"') {
            $hasIncluded = $true
        }
        
        if ($line -match '#include "3D/Trail.h"' -and -not $hasIncluded) {
            $newLines.Add($line)
            $newLines.Add('#include "EnvironmentRenderer.h"')
            continue
        }
        
        $newLines.Add($line)
    }
    
    [IO.File]::WriteAllLines($filepath, $newLines.ToArray(), $utf8NoBom)
}

function ProcessCpp($filepath) {
    $lines = [IO.File]::ReadAllLines($filepath, $utf8NoBom)
    $newLines = [System.Collections.ArrayList]::new()
    $skip = $false
    
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]
        
        if ($line -match 'skybox = std::make_unique<Skybox>') { $skip = $true }
        if ($line -match 'particleEmitter = std::make_unique<ParticleEmitter>') { $skip = $true }
        if ($line -match 'myRing = std::make_unique<Primitive>') { $skip = $true }
        if ($line -match 'myPartialRing = std::make_unique<Primitive>') { $skip = $true }
        if ($line -match 'myCylinder = std::make_unique<Primitive>') { $skip = $true }
        if ($line -match 'boundaryAlertPlane_ = std::make_unique<Primitive>') { $skip = $true }
        
        if ($line -match 'if \(myRing && showNormalRing\) \{') { $skip = $true; continue }
        if ($line -match 'if \(myPartialRing && showPartialRing\) \{') { $skip = $true; continue }
        if ($line -match 'if \(myCylinder && showCylinder\) \{') { $skip = $true; continue }
        
        if ($skip) {
            if ($line.Trim() -eq "}" -or $line.Trim() -eq "") {
                if ($i -lt ($lines.Length - 1) -and $lines[$i+1].Trim() -ne "") {
                    $skip = $false
                }
            }
            continue
        }
        
        # Skip drawing
        if ($line -match 'if \(myRing && showNormalRing\) myRing->Draw\(\);') { continue }
        if ($line -match 'if \(myPartialRing && showPartialRing\) myPartialRing->Draw\(\);') { continue }
        if ($line -match 'if \(myCylinder && showCylinder\) myCylinder->Draw\(\);') { continue }
        
        if ($line -match 'if \(particleManager\) \{' -and $lines[$i+1] -match 'Draw') { continue }
        if ($line -match 'particleManager->Draw\(\);' -and $lines[$i-1] -match 'if \(particleManager\)') { continue }
        if ($line.Trim() -eq "}" -and $lines[$i-1] -match 'particleManager->Draw\(\);') { continue }
        
        if ($line -match 'if \(skybox\) \{' -and $lines[$i+1] -match 'Draw') { continue }
        if ($line -match 'skybox->Draw\(\);' -and $lines[$i-1] -match 'if \(skybox\)') { continue }
        if ($line.Trim() -eq "}" -and $lines[$i-1] -match 'skybox->Draw\(\);') { continue }
        
        if ($line -match 'skybox->Update\(') { continue }
        if ($line -match 'particleManager->Update\(') { continue }
        if ($line -match 'particleEmitter->Update\(') { continue }
        
        if ($line -match 'if \(explosionManager_\) explosionManager_->Draw\(\);') {
            $newLines.Add("`tif (environmentRenderer_) environmentRenderer_->Draw();")
        }
        
        if ($line -match 'environmentRenderer_ = std::make_unique<EnvironmentRenderer>\(\);') {
            $newLines.Add($line)
            $newLines.Add("`tenvironmentRenderer_->Initialize();")
            continue
        }
        
        if ($line -match 'uiManager_->UpdateUI\(\);') {
            $newLines.Add("`tif (environmentRenderer_) environmentRenderer_->Update(camera.get());")
            $newLines.Add($line)
            continue
        }

        $newLines.Add($line)
    }
    
    [IO.File]::WriteAllLines($filepath, $newLines.ToArray(), $utf8NoBom)
}

ProcessHeader "project\Game\Scene\GamePlayScene.h"
ProcessCpp "project\Game\Scene\GamePlayScene.cpp"
Write-Host "Done!"
