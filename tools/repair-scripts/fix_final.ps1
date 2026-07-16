param()
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$textScene = [IO.File]::ReadAllText($sceneCpp, $utf8NoBom)
$textScene = [regex]::Replace($textScene, '(?m)^\s*myRing = std::make_unique<Primitive>\(\);\r?\n^\s*myRing->Initialize.*?;\r?\n^\s*myRing->SetTranslate.*?;\r?\n', '')
[IO.File]::WriteAllText($sceneCpp, $textScene, $utf8NoBom)
Write-Host "GamePlayScene.cpp fixed"

$envCpp = "project\Game\Scene\EnvironmentRenderer.cpp"
$textEnv = [IO.File]::ReadAllText($envCpp, $utf8NoBom)
$textEnv = $textEnv.Replace("Object3dCommon::GetInstance()->SetDefaultDrawSettings();", "// Object3dCommon::GetInstance()->SetCommonDrawSettings();")
[IO.File]::WriteAllText($envCpp, $textEnv, $utf8NoBom)
Write-Host "EnvironmentRenderer.cpp fixed"
