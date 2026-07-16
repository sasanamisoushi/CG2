param()
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

$sceneCpp = "project\Game\Scene\GamePlayScene.cpp"
$lines = [IO.File]::ReadAllLines($sceneCpp, $utf8NoBom)
$newLines = [System.Collections.ArrayList]::new()

for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($lines[$i] -match "myRing = std::make_unique<Primitive>\(\);") {
        # skip this and next 2 lines
        $i += 2
        continue
    }
    $newLines.Add($lines[$i])
}

[IO.File]::WriteAllLines($sceneCpp, $newLines.ToArray(), $utf8NoBom)
Write-Host "GamePlayScene.cpp fixed myRing manually"
