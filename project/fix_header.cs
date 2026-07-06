using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.h";
        string content = File.ReadAllText(path);

        content = content.Replace("bool showPlane = false;", "bool showPlane = true;");
        content = content.Replace("bool showSkybox = false;", "bool showSkybox = true;");

        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done Header Fix");
    }
}
