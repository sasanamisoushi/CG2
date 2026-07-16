param()

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

# Update GamePlayScene.cpp to instantiate environmentRenderer_
$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$lines = [IO.File]::ReadAllLines($sceneCpp, $utf8NoBom)
$newLines = [System.Collections.ArrayList]::new()
foreach ($line in $lines) {
    $newLines.Add($line)
    if ($line -match 'uiManager_ = std::make_unique<GamePlayUIManager>\(this\);') {
        $newLines.Add("`tenvironmentRenderer_ = std::make_unique<EnvironmentRenderer>();")
        $newLines.Add("`tenvironmentRenderer_->Initialize();")
    }
}
[IO.File]::WriteAllLines($sceneCpp, $newLines.ToArray(), $utf8NoBom)


# Update GamePlayUIManager.cpp macros
$uiCpp = "project\Game\Scene\GamePlayUIManager.cpp"
$text = [IO.File]::ReadAllText($uiCpp, $utf8NoBom)

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
    # Replace "#define X scene_->X" with "#define X scene_->environmentRenderer_->X_"
    $text = $text.Replace("#define $r scene_->$r", "#define $r scene_->environmentRenderer_->${r}_")
}

# Also update GamePlayScene.h to remove showNormalRing etc if they were missed
# Actually they were removed by the previous script

[IO.File]::WriteAllText($uiCpp, $text, $utf8NoBom)
Write-Host "Updated UI Manager macros!"
