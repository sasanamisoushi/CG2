param()
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($False)

$hFile = "project\Game\Scene\GamePlayUIManager.h"
$text = [IO.File]::ReadAllText($hFile, $utf8NoBom)

$insertMethods = @"
	void DrawGameplayActionControls();
"@

$insertVars = @"
	bool showSimulationWindow_ = false;
	int currentSimulationTarget_ = 0;
	std::string simulationSaveMessage_;
	char simulationActionName_[64] = "Action1";
	std::vector<std::string> simulationActionNames_;
	int selectedSimulationActionIndex_ = 0;
	std::string simulationActionMessage_;
	int simulationPlaybackMode_ = 0;
	char missilePresetName_[64] = "MissilePreset1";
	int missilePresetTypeIndex_ = 0;
	std::vector<std::string> missilePresetNames_[2];
	int selectedMissilePresetIndex_[2] = { 0, 0 };
	std::string missilePresetMessage_;
"@

$text = $text.Replace("	void UpdateUI();", "	void UpdateUI();`r`n`r`n$insertMethods")
$text = $text.Replace("	GamePlayScene* scene_ = nullptr;", "	GamePlayScene* scene_ = nullptr;`r`n`r`n$insertVars")

[IO.File]::WriteAllText($hFile, $text, $utf8NoBom)
Write-Host "GamePlayUIManager.h updated"
