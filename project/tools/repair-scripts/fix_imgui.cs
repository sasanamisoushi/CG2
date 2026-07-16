using System;
using System.IO;
using System.Text;

class Program {
    static void Main() {
        string path = @"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp_restored";
        string content = File.ReadAllText(path, Encoding.UTF8);
        content = content.Replace("#ifdef ENABLE_IMGUI", "#if 0");
        content = content.Replace("#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)", "#if 0");
        
        string outPath = @"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        File.WriteAllText(outPath, content, new UTF8Encoding(true));
        Console.WriteLine("Done. Length: " + content.Length);
    }
}
