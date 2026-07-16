using System;
using System.IO;
using System.Text.RegularExpressions;
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        string path = @"project\Game\Scene\GamePlayScene.cpp";
        string content = File.ReadAllText(path);
        
        string[] funcs = {
            "SaveCurrentSimulationLayoutToSceneJson",
            "RefreshSimulationActionNames",
            "SaveNamedSimulationAction",
            "ApplySimulationAction",
            "DrawSimulationScreenUI",
            "DrawSimulationSaveControls",
            "RefreshMissilePresetNames",
            "SaveMissilePreset",
            "ApplyMissilePreset",
            "DrawMissileSettingsUI",
            "FirePlayerMissile",
            "UpdateLockOn",
            "FindLockOnTarget",
            "IsLockedEnemyAlive",
            "FindAimAssistTarget",
            "FindMultiLockTarget",
            "BeginMultiLock",
            "PruneMultiLockTargets",
            "UpdateMultiLock",
            "FireMultiLockMissiles",
            "CancelMultiLock"
        };
        
        foreach (string func in funcs)
        {
            string pattern = @"^[\w\s\*&]*\s+GamePlayScene::" + func + @"\s*\([^)]*\)\s*(const)?\s*\{";
            Match match = Regex.Match(content, pattern, RegexOptions.Multiline);
            if (match.Success)
            {
                int startIndex = match.Index;
                int braceCount = 0;
                bool inString = false, inChar = false, inComment = false, inLineComment = false;
                int endIndex = -1;
                
                for (int i = match.Index + match.Length - 1; i < content.Length; i++)
                {
                    char c = content[i];
                    char next = i + 1 < content.Length ? content[i+1] : '\0';
                    char prev = i - 1 >= 0 ? content[i-1] : '\0';
                    
                    if (inLineComment) { if (c == '\n') inLineComment = false; continue; }
                    if (inComment) { if (c == '*' && next == '/') { inComment = false; i++; } continue; }
                    if (inString) { if (c == '"' && prev != '\\') inString = false; continue; }
                    if (inChar) { if (c == '\'' && prev != '\\') inChar = false; continue; }
                    
                    if (c == '/' && next == '/') { inLineComment = true; i++; continue; }
                    if (c == '/' && next == '*') { inComment = true; i++; continue; }
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
                
                if (endIndex != -1)
                {
                    string extracted = content.Substring(startIndex, endIndex - startIndex);
                    File.AppendAllText("extracted_funcs.txt", "// --- " + func + " ---\n" + extracted + "\n\n");
                    content = content.Remove(startIndex, endIndex - startIndex);
                }
            }
        }
        File.WriteAllText(@"project\Game\Scene\GamePlayScene_reduced.cpp", content);
    }
}
