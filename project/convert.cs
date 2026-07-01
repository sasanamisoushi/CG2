using System;
using System.IO;
using System.Text;

class Program {
    static void Main() {
        string path = @"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp_restored";
        string outPath = @"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp_sjis_test.cpp";
        string content = File.ReadAllText(path, Encoding.GetEncoding(932)); // Shift-JIS
        File.WriteAllText(outPath, content, new UTF8Encoding(true)); // UTF-8 with BOM
        Console.WriteLine("Converted successfully. Length: " + content.Length);
    }
}
