param()
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

$cppFile = "project\Game\Scene\GamePlayUIManager.cpp"
$text = [IO.File]::ReadAllText($cppFile, $utf8NoBom)

$macrosToRemove = @(
	"showSimulationWindow_",
	"currentSimulationTarget_",
	"simulationSaveMessage_",
	"simulationActionName_",
	"simulationActionNames_",
	"selectedSimulationActionIndex_",
	"simulationActionMessage_",
	"simulationPlaybackMode_",
	"missilePresetName_",
	"missilePresetTypeIndex_",
	"missilePresetNames_",
	"selectedMissilePresetIndex_",
	"missilePresetMessage_",
	"DrawGameplayActionControls"
)

foreach ($macro in $macrosToRemove) {
	$text = [regex]::Replace($text, "(?m)^\s*#define $macro.*\r?\n", "")
	$text = [regex]::Replace($text, "(?m)^\s*#undef $macro.*\r?\n", "")
}

[IO.File]::WriteAllText($cppFile, $text, $utf8NoBom)
Write-Host "GamePlayUIManager.cpp macros updated"
