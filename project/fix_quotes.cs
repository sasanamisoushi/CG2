using System;
using System.IO;
using System.Text;

class Program {
    static void Main() {
        string path = @"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string content = File.ReadAllText(path, Encoding.UTF8);
        content = content.Replace("、E;", "。\";");
        content = content.Replace("、E)", "。\")");
        content = content.Replace("、E +", "、「\" +");
        content = content.Replace("、E,", "。\",");
        File.WriteAllText(path, content, new UTF8Encoding(true));
        Console.WriteLine("Fixed missing quotes.");
    }
}
