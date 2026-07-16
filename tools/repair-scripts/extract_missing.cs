using System;
using System.IO;
using System.Text.RegularExpressions;
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        string[] missing = { "FindLockOnTarget", "IsLockedEnemyAlive", "FindAimAssistTarget", "FindMultiLockTarget", "BeginMultiLock" };
        string cppPath = @"project\Game\Scene\GamePlayScene.cpp";
        string cppContent = File.ReadAllText(cppPath);
        
        var methodsCode = new Dictionary<string, string>();
        
        foreach (string method in missing)
        {
            string pattern = @"(?m)^[\w\s\*&<>\:]*?\s*GamePlayScene::" + method + @"\s*\([\s\S]*?\)\s*(const)?\s*\{";
            Match match = Regex.Match(cppContent, pattern);
            if (match.Success)
            {
                int startIndex = match.Index;
                int braceCount = 0;
                int endIndex = -1;
                bool inString = false;
                bool inChar = false;
                bool inComment = false;
                bool inBlockComment = false;

                for (int i = startIndex; i < cppContent.Length; i++)
                {
                    char c = cppContent[i];
                    char nextC = i + 1 < cppContent.Length ? cppContent[i + 1] : '\0';

                    if (!inComment && !inBlockComment && !inString && !inChar)
                    {
                        if (c == '/' && nextC == '/') { inComment = true; i++; continue; }
                        if (c == '/' && nextC == '*') { inBlockComment = true; i++; continue; }
                        if (c == '"') { inString = true; continue; }
                        if (c == '\'') { inChar = true; continue; }
                        
                        if (c == '{') braceCount++;
                        else if (c == '}')
                        {
                            braceCount--;
                            if (braceCount == 0)
                            {
                                endIndex = i + 1;
                                break;
                            }
                        }
                    }
                    else if (inString)
                    {
                        if (c == '\\') i++;
                        else if (c == '"') inString = false;
                    }
                    else if (inChar)
                    {
                        if (c == '\\') i++;
                        else if (c == '\'') inChar = false;
                    }
                    else if (inComment)
                    {
                        if (c == '\n') inComment = false;
                    }
                    else if (inBlockComment)
                    {
                        if (c == '*' && nextC == '/') { inBlockComment = false; i++; }
                    }
                }
                
                if (endIndex != -1)
                {
                    string methodBody = cppContent.Substring(startIndex, endIndex - startIndex);
                    methodsCode[method] = methodBody;
                    cppContent = cppContent.Remove(startIndex, endIndex - startIndex);
                }
            }
        }
        
        // Write the extracted methods to LockOnManager.cpp and modify them
        string lockOnPath = @"project\Game\Scene\LockOnManager.cpp";
        string lockOnContent = File.ReadAllText(lockOnPath);
        
        var sceneMembers = new[] {
            "player_", "enemies_", "obstacles_", "enemySpawns_", "missileManager_", "explosionManager_", "camera", "debugFlyCamera_", 
            "isDebugCameraActive_", "lockedEnemy_", "aimAssistEnemy_", "isCinematicLockOnCameraInitialized_", "isGameOver_", "gameOverTimer_", 
            "missileNormalSpeed", "missileNormalScale", "missileNormalCollisionRadius", "missileNormalLifeTime", "missileSpeed", 
            "missileAmpX", "missileAmpZ", "missileAmpY", "missileFreqY", "missileBaseY", "missileHomingStrength", "missileHomingScale", 
            "missileHomingCollisionRadius", "missileTrailWidth", "missileLifeTime", "missileMuzzleOffset", "lastJsonWriteTime_", 
            "isEditorPreviewPlaying_", "simulationSaveMessage_", "simulationActionName_", "simulationActionNames_", "selectedSimulationActionIndex_", 
            "simulationActionMessage_", "simulationPlaybackMode_", "currentSimulationTarget_", "missilePresetName_", "missilePresetTypeIndex_", 
            "missilePresetNames_", "selectedMissilePresetIndex_", "missilePresetMessage_", "multiLockTargets_", "isMultiLockCharging_", 
            "multiLockChargeFrames_", "enemyRespawnTimers_", "enemyEventManager_", "newEnemyPos"
        };
        var helpers = new[] {
            "ReadVector3Json", "ApplyPlayerModeParamsFromJson",
            "ReadJsonInt", "ReadJsonFloat", "PlayerModeParamsToJson", "EulerFromQuaternionXYZ", "ToVector3Json", "MakeUniqueSceneObjectName", "WriteSceneTransform",
            "IsScenePlayerObject", "IsSceneEnemyObject", "IsSceneObstacleObject"
        };
        var actualMembers = new[] { "SetDebugCameraActive", "ResetEditorPreview", "CancelMultiLock" };
        var lockOnMethods = new[] { "UpdateLockOn", "FindLockOnTarget", "IsLockedEnemyAlive", "FindAimAssistTarget", "FindMultiLockTarget", "BeginMultiLock", "PruneMultiLockTargets", "UpdateMultiLock", "FireMultiLockMissiles", "CancelMultiLock" };
        
        foreach (var method in missing)
        {
            if (methodsCode.ContainsKey(method))
            {
                string code = methodsCode[method];
                code = Regex.Replace(code, @"\bGamePlayScene::" + method + @"\b", "LockOnManager::" + method);
                
                foreach (string member in sceneMembers)
                {
                    code = Regex.Replace(code, @"\b" + member + @"\b", "scene_->" + member);
                }
                foreach (string h in helpers)
                {
                    code = Regex.Replace(code, @"(?<!->|\.|::)\b" + h + @"\s*\(", h + "(");
                }
                foreach (string a in actualMembers)
                {
                    code = Regex.Replace(code, @"(?<!->|\.|::)\b" + a + @"\s*\(", "scene_->" + a + "(");
                }
                foreach (string m in lockOnMethods)
                {
                    code = Regex.Replace(code, @"\bscene_->" + m + @"\s*\(", m + "(");
                }
                
                lockOnContent += "\n\n" + code;
            }
        }
        
        File.WriteAllText(lockOnPath, lockOnContent);
        
        // Fix calls in GamePlayScene.cpp
        cppContent = Regex.Replace(cppContent, @"\bFindLockOnTarget\b", "lockOnManager_->FindLockOnTarget");
        cppContent = Regex.Replace(cppContent, @"\bIsLockedEnemyAlive\b", "lockOnManager_->IsLockedEnemyAlive");
        cppContent = Regex.Replace(cppContent, @"\bFindAimAssistTarget\b", "lockOnManager_->FindAimAssistTarget");
        cppContent = Regex.Replace(cppContent, @"\bFindMultiLockTarget\b", "lockOnManager_->FindMultiLockTarget");
        cppContent = Regex.Replace(cppContent, @"\bBeginMultiLock\b", "lockOnManager_->BeginMultiLock");
        
        cppContent = Regex.Replace(cppContent, @"\bSaveCurrentSimulationLayoutToSceneJson\b", "simulationManager_->SaveCurrentSimulationLayoutToSceneJson");
        cppContent = Regex.Replace(cppContent, @"\bSaveNamedSimulationAction\b", "simulationManager_->SaveNamedSimulationAction");
        cppContent = Regex.Replace(cppContent, @"\bApplySimulationAction\b", "simulationManager_->ApplySimulationAction");
        cppContent = Regex.Replace(cppContent, @"\bDrawSimulationSaveControls\b", "simulationManager_->DrawSimulationSaveControls");
        
        cppContent = Regex.Replace(cppContent, @"\bRefreshMissilePresetNames\b", "missilePresetManager_->RefreshMissilePresetNames");
        cppContent = Regex.Replace(cppContent, @"\bSaveMissilePreset\b", "missilePresetManager_->SaveMissilePreset");
        cppContent = Regex.Replace(cppContent, @"\bApplyMissilePreset\b", "missilePresetManager_->ApplyMissilePreset");
        
        // Also need to add declarations of missing methods to LockOnManager.h
        string hPath = @"project\Game\Scene\LockOnManager.h";
        string hContent = File.ReadAllText(hPath);
        
        foreach (var method in missing)
        {
            if (methodsCode.ContainsKey(method))
            {
                Match sigMatch = Regex.Match(methodsCode[method], @"^([\w\s\*&]+)\s+GamePlayScene::(" + method + @"\s*\([\s\S]*?\)\s*(const)?)");
                if (sigMatch.Success)
                {
                    string returnType = sigMatch.Groups[1].Value.Trim();
                    string methodSig = sigMatch.Groups[2].Value.Trim();
                    // Clean up newlines in methodSig
                    methodSig = Regex.Replace(methodSig, @"\s+", " ");
                    hContent = hContent.Replace("public:", "public:\n\t" + returnType + " " + methodSig + ";");
                }
            }
        }
        File.WriteAllText(hPath, hContent);
        
        File.WriteAllText(cppPath, cppContent);
        Console.WriteLine("Missing methods extracted and calls updated.");
    }
}
