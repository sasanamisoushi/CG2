using System;
using System.IO;
using System.Text.RegularExpressions;
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        string cppFile = @"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string[] cppLines = File.ReadAllLines(cppFile);
        
        for (int i = 0; i < cppLines.Length; i++)
        {
            string line = cppLines[i];
            
            // Fix GamePlayScene(Mode mode)
            if (line.Contains("GamePlayScene::GamePlayScene(Mode mode)"))
            {
                cppLines[i] = line.Replace("Mode mode", "GamePlayScene::Mode mode");
            }
            
            // Fix syntax errors around ImGui::Button and ImGui::Combo
            if (line.Contains("if (ImGui::Button(\"") && line.EndsWith("{") && !line.Contains("))"))
            {
                cppLines[i] = "\t\tif (ImGui::Button(\"Button\")) {";
            }
            if (line.Contains("ImGui::Combo(\"") && (line.EndsWith(");") || line.EndsWith("\");")))
            {
                if (line.Contains("simulationActionNames_"))
                {
                    cppLines[i] = "\t\tImGui::Combo(\"Combo\", &selectedSimulationActionIndex_, actionNameItems.data(), static_cast<int>(actionNameItems.size()));";
                }
                else if (line.Contains("missilePresetTypeIndex_"))
                {
                    cppLines[i] = "\t\tImGui::Combo(\"Combo2\", &missilePresetTypeIndex_, presetTypes, IM_ARRAYSIZE(presetTypes));";
                }
                else
                {
                    cppLines[i] = "// " + line;
                }
            }
            
            // Fix invalid identifier C3872 / C3688
            if (line.Contains("simulationActionMessage_ = ") && line.Contains("scene.json"))
            {
                cppLines[i] = "\t\t\tsimulationActionMessage_ = \"scene.json\";";
            }
        }
        File.WriteAllLines(cppFile, cppLines);
        
        string hFile = @"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.h";
        string hContent = File.ReadAllText(hFile);
        if (!hContent.Contains("aimCursorSprite_"))
        {
            hContent = hContent.Replace("std::unique_ptr<Sprite> sprite;", "std::unique_ptr<Sprite> sprite;\n\tstd::unique_ptr<Sprite> aimCursorSprite_;\n\tstd::unique_ptr<Sprite> lockOnReticleSprite_;");
            File.WriteAllText(hFile, hContent);
        }
    }
}
