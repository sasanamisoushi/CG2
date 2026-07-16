using System;
using System.IO;
using System.Text.RegularExpressions;

class Program
{
    static void Main()
    {
        string[] managers = { "SimulationManager", "MissilePresetManager", "LockOnManager" };
        string[] helpers = {
            "SetDebugCameraActive", "ResetEditorPreview", "CancelMultiLock", "TrimActionName", "ReadVector3Json", "ApplyPlayerModeParamsFromJson",
            "ReadJsonInt", "ReadJsonFloat", "PlayerModeParamsToJson", "EulerFromQuaternionXYZ", "ToVector3Json", "MakeUniqueSceneObjectName", "WriteSceneTransform",
            "IsScenePlayerObject", "IsSceneEnemyObject", "IsSceneObstacleObject"
        };
        
        var actualMembers = new System.Collections.Generic.HashSet<string> { "SetDebugCameraActive", "ResetEditorPreview", "CancelMultiLock" };
        
        foreach (string manager in managers)
        {
            string cppPath = string.Format(@"project\Game\Scene\{0}.cpp", manager);
            if (!File.Exists(cppPath)) continue;
            
            string content = File.ReadAllText(cppPath);
            if (!content.Contains("using json = nlohmann::json;"))
            {
                content = content.Replace("#include \"engine/Math/MyMath.h\"", "#include \"engine/Math/MyMath.h\"\n#include \"GamePlaySceneHelpers.h\"\n\nusing json = nlohmann::json;");
            }
            
            foreach (string helper in helpers)
            {
                if (!actualMembers.Contains(helper))
                {
                    content = content.Replace("scene_->" + helper, helper);
                }
            }
            
            File.WriteAllText(cppPath, content);
        }
        
        string sceneCpp = File.ReadAllText(@"project\Game\Scene\GamePlayScene.cpp");
        int nsStart = sceneCpp.IndexOf("namespace {");
        if (nsStart != -1)
        {
            int braceCount = 0;
            int nsEnd = -1;
            for (int i = nsStart + 10; i < sceneCpp.Length; i++)
            {
                if (sceneCpp[i] == '{') braceCount++;
                else if (sceneCpp[i] == '}')
                {
                    braceCount--;
                    if (braceCount == 0)
                    {
                        nsEnd = i + 1;
                        break;
                    }
                }
            }
            
            if (nsEnd != -1)
            {
                string nsContent = sceneCpp.Substring(nsStart, nsEnd - nsStart);
                sceneCpp = sceneCpp.Remove(nsStart, nsEnd - nsStart);
                sceneCpp = sceneCpp.Insert(nsStart, "#include \"GamePlaySceneHelpers.h\"\n");
                
                File.WriteAllText(@"project\Game\Scene\GamePlayScene.cpp", sceneCpp);
                
                string helpersH = "#pragma once\n#include \"engine/Math/Vector3.h\"\n#include \"engine/Math/Quaternion.h\"\n#include \"externals/json.hpp\"\n#include \"Game/Player/Player.h\"\n\nusing json = nlohmann::json;\n\n" + nsContent;
                File.WriteAllText(@"project\Game\Scene\GamePlaySceneHelpers.h", helpersH);
            }
        }
        
        string projPath = @"project\CG2.vcxproj";
        string filtersPath = @"project\CG2.vcxproj.filters";
        string proj = File.ReadAllText(projPath);
        string filters = File.ReadAllText(filtersPath);
        if (!proj.Contains("GamePlaySceneHelpers.h"))
        {
            proj = Regex.Replace(proj, @"(<ClInclude Include=""Game\\Scene\\GamePlayScene.h""\s*/>)", "$1\n    <ClInclude Include=\"Game\\Scene\\GamePlaySceneHelpers.h\" />");
            filters = Regex.Replace(filters, @"(<ClInclude Include=""Game\\Scene\\GamePlayScene.h"">[\s\S]*?</ClInclude>)", "$1\n    <ClInclude Include=\"Game\\Scene\\GamePlaySceneHelpers.h\">\n      <Filter>Game\\Scene</Filter>\n    </ClInclude>");
            File.WriteAllText(projPath, proj);
            File.WriteAllText(filtersPath, filters);
        }
        
        Console.WriteLine("Fixed build issues.");
    }
}
