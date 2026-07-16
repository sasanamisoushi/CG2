using System;
using System.IO;
using System.Text.RegularExpressions;

class Program
{
    static void Main()
    {
        string projPath = @"project\CG2.vcxproj";
        string filtersPath = @"project\CG2.vcxproj.filters";
        
        string proj = File.ReadAllText(projPath);
        string filters = File.ReadAllText(filtersPath);
        
        string[] hFiles = { @"Game\Scene\SimulationManager.h", @"Game\Scene\MissilePresetManager.h", @"Game\Scene\LockOnManager.h" };
        string[] cppFiles = { @"Game\Scene\SimulationManager.cpp", @"Game\Scene\MissilePresetManager.cpp", @"Game\Scene\LockOnManager.cpp" };
        
        foreach (var h in hFiles)
        {
            if (!proj.Contains(h))
            {
                proj = Regex.Replace(proj, @"(<ClInclude Include=""Game\\Scene\\GamePlayScene.h""\s*/>)", "$1\n    <ClInclude Include=\"" + h + "\" />");
                filters = Regex.Replace(filters, @"(<ClInclude Include=""Game\\Scene\\GamePlayScene.h"">[\s\S]*?</ClInclude>)", "$1\n    <ClInclude Include=\"" + h + "\">\n      <Filter>Game\\Scene</Filter>\n    </ClInclude>");
            }
        }
        
        foreach (var cpp in cppFiles)
        {
            if (!proj.Contains(cpp))
            {
                proj = Regex.Replace(proj, @"(<ClCompile Include=""Game\\Scene\\GamePlayScene.cpp""\s*/>)", "$1\n    <ClCompile Include=\"" + cpp + "\" />");
                filters = Regex.Replace(filters, @"(<ClCompile Include=""Game\\Scene\\GamePlayScene.cpp"">[\s\S]*?</ClCompile>)", "$1\n    <ClCompile Include=\"" + cpp + "\">\n      <Filter>Game\\Scene</Filter>\n    </ClCompile>");
            }
        }
        
        File.WriteAllText(projPath, proj);
        File.WriteAllText(filtersPath, filters);
        Console.WriteLine("Updated vcxproj and filters.");
    }
}
