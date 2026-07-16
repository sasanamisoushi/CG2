param()

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$text = [IO.File]::ReadAllText($sceneCpp, $utf8NoBom)

# Remove the primitive initializations
$text = [regex]::Replace($text, '(?m)^\s*myRing = std::make_unique<Primitive>\(\);\r?\n^\s*myRing->Initialize.*?;\r?\n^\s*myRing->SetTranslate.*?;\r?\n', '')
$text = [regex]::Replace($text, '(?m)^\s*myPartialRing = std::make_unique<Primitive>\(\);\r?\n^\s*myPartialRing->Initialize.*?;\r?\n^\s*myPartialRing->SetTranslate.*?;\r?\n', '')
$text = [regex]::Replace($text, '(?m)^\s*myCylinder = std::make_unique<Primitive>\(\);\r?\n^\s*myCylinder->Initialize.*?;\r?\n^\s*myCylinder->SetTranslate.*?;\r?\n^\s*myCylinder->SetScale.*?;\r?\n', '')
$text = [regex]::Replace($text, '(?m)^\s*boundaryAlertPlane_ = std::make_unique<Primitive>\(\);\r?\n^\s*boundaryAlertPlane_->Initialize.*?;\r?\n', '')

# Remove particle initialization
$text = [regex]::Replace($text, '(?m)^\s*particleManager = std::make_unique<ParticleManager>\(\);\r?\n^\s*particleManager->Initialize.*?;\r?\n', '')
$text = [regex]::Replace($text, '(?m)^\s*particleEmitter = std::make_unique<ParticleEmitter>.*?;\r?\n', '')

# Remove skybox initialization
$text = [regex]::Replace($text, '(?m)^\s*skybox = std::make_unique<Skybox>\(\);\r?\n^\s*skybox->Initialize.*?;\r?\n', '')

# Remove the updates in Update()
$text = [regex]::Replace($text, '(?s)\s*if \(myRing && showNormalRing\) \{.*?myRing->Update\(\);\s*\}', '')
$text = [regex]::Replace($text, '(?s)\s*if \(myPartialRing && showPartialRing\) \{.*?myPartialRing->Update\(\);\s*\}', '')
$text = [regex]::Replace($text, '(?s)\s*if \(myCylinder && showCylinder\) \{.*?myCylinder->Update\(\);\s*\}', '')

$text = [regex]::Replace($text, '(?m)^\s*skybox->Update\(.*?\);\r?\n', '')
$text = [regex]::Replace($text, '(?m)^\s*particleManager->Update\(.*?\);\r?\n', '')
$text = [regex]::Replace($text, '(?m)^\s*particleEmitter->Update\(\);\r?\n', '')

# Remove draws in Draw()
$text = [regex]::Replace($text, '(?m)^\s*if \(myRing && showNormalRing\) myRing->Draw\(\);\r?\n', '')
$text = [regex]::Replace($text, '(?m)^\s*if \(myPartialRing && showPartialRing\) myPartialRing->Draw\(\);\r?\n', '')
$text = [regex]::Replace($text, '(?m)^\s*if \(myCylinder && showCylinder\) myCylinder->Draw\(\);\r?\n', '')

$text = [regex]::Replace($text, '(?s)\s*if \(particleManager\) \{\s*particleManager->Draw\(\);\s*\}', '')
$text = [regex]::Replace($text, '(?s)\s*if \(skybox\) \{\s*skybox->Draw\(\);\s*\}', '')

# Add EnvironmentRenderer calls
# Init
$text = $text.Replace("uiManager_ = std::make_unique<GamePlayUIManager>(this);", "uiManager_ = std::make_unique<GamePlayUIManager>(this);`n`tenvironmentRenderer_ = std::make_unique<EnvironmentRenderer>();`n`tenvironmentRenderer_->Initialize();")
# Update
$text = $text.Replace("uiManager_->UpdateUI();", "if (environmentRenderer_) environmentRenderer_->Update(camera.get());`n`t`tuiManager_->UpdateUI();")
# Draw
$text = $text.Replace("if (explosionManager_) explosionManager_->Draw();", "if (environmentRenderer_) environmentRenderer_->Draw();`n`t`tif (explosionManager_) explosionManager_->Draw();")

[IO.File]::WriteAllText($sceneCpp, $text, $utf8NoBom)
Write-Host "GamePlayScene.cpp done!"

# Update GamePlayScene.h
$sceneH = "project\Game\Scene\GamePlayScene.h"
$textH = [IO.File]::ReadAllText($sceneH, $utf8NoBom)
$textH = [regex]::Replace($textH, '(?m)^\s*int\s+prSubdivision.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+prOuterRadius.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+prInnerRadius.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*bool\s+prIsUvHorizontal.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+prInnerColor.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+prOuterColor.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+prStartAngle.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+prEndAngle.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+prFadeAngle.*?;\r?\n', '')

$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderPos.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderScale.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderUVOffset.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderUVScrollSpeed.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderAlphaReference.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*int\s+cylinderSubdivision.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*int\s+cylinderVerticalSubdivision.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderTopRadiusX.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderTopRadiusZ.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderBottomRadiusX.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderBottomRadiusZ.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderHeight.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderTopColor.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderBottomColor.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderStartAngle.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*float\s+cylinderEndAngle.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*bool\s+cylinderIsUvFlipped.*?;\r?\n', '')

$textH = [regex]::Replace($textH, '(?m)^\s*bool\s+showNormalRing.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*bool\s+showPartialRing.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*bool\s+showCylinder.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*bool\s+showSkybox.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*bool\s+showParticles.*?;\r?\n', '')

$textH = [regex]::Replace($textH, '(?m)^\s*std::unique_ptr<Skybox>\s+skybox.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*std::unique_ptr<ParticleManager>\s+particleManager.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*std::unique_ptr<ParticleEmitter>\s+particleEmitter.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*std::unique_ptr<Primitive>\s+boundaryAlertPlane_.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*std::unique_ptr<Primitive>\s+myRing.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*std::unique_ptr<Primitive>\s+myPartialRing.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*std::unique_ptr<Primitive>\s+myCylinder.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*std::unique_ptr<Trail>\s+missileTrail.*?;\r?\n', '')
$textH = [regex]::Replace($textH, '(?m)^\s*std::unique_ptr<Object3d>\s+trailObject.*?;\r?\n', '')

if ($textH -notmatch '#include "EnvironmentRenderer.h"') {
    $textH = $textH.Replace('#include "3D/Trail.h"', "#include `"3D/Trail.h`"`n#include `"EnvironmentRenderer.h`"")
}

[IO.File]::WriteAllText($sceneH, $textH, $utf8NoBom)
Write-Host "GamePlayScene.h done!"

# Update GamePlayUIManager.cpp macros
$uiCpp = "project\Game\Scene\GamePlayUIManager.cpp"
$textUI = [IO.File]::ReadAllText($uiCpp, $utf8NoBom)

$replacements = @(
    "showNormalRing", "showPartialRing", "showCylinder", 
    "prSubdivision", "prOuterRadius", "prInnerRadius", "prIsUvHorizontal",
    "prInnerColor", "prOuterColor", "prStartAngle", "prEndAngle", "prFadeAngle",
    "cylinderPos", "cylinderScale", "cylinderUVOffset", "cylinderUVScrollSpeed",
    "cylinderAlphaReference", "cylinderSubdivision", "cylinderVerticalSubdivision",
    "cylinderTopRadiusX", "cylinderTopRadiusZ", "cylinderBottomRadiusX", "cylinderBottomRadiusZ",
    "cylinderHeight", "cylinderTopColor", "cylinderBottomColor", "cylinderStartAngle",
    "cylinderEndAngle", "cylinderIsUvFlipped"
)

foreach ($r in $replacements) {
    if ($textUI -match "#define $r scene_->$r\b") {
        $textUI = $textUI.Replace("#define $r scene_->$r", "#define $r scene_->environmentRenderer_->${r}_")
    }
}
[IO.File]::WriteAllText($uiCpp, $textUI, $utf8NoBom)
Write-Host "GamePlayUIManager.cpp done!"
