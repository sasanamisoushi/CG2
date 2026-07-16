using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string[] lines = File.ReadAllLines(path);
        
        bool found = false;
        using (StreamWriter sw = new StreamWriter(path, false, new System.Text.UTF8Encoding(true))) {
            for (int i = 0; i < lines.Length; i++) {
                if (lines[i].Contains("Vector3 fireDirection = forward;") && 
                    i + 1 < lines.Length && lines[i+1].Contains("if (target && type == MissileType::MissileWithTrail) {")) {
                    
                    sw.WriteLine("\tVector3 fireDirection = forward;");
                    sw.WriteLine("\tbool shouldAim = false;");
                    sw.WriteLine("\tif (target) {");
                    sw.WriteLine("\t\tif (type == MissileType::MissileWithTrail) {");
                    sw.WriteLine("\t\t\tshouldAim = true;");
                    sw.WriteLine("\t\t} else if (type == MissileType::Normal) {");
                    sw.WriteLine("\t\t\tPlayerMode mode = player_->GetCurrentMode();");
                    sw.WriteLine("\t\t\tif (mode == PlayerMode::Gerwalk || mode == PlayerMode::Battroid) {");
                    sw.WriteLine("\t\t\t\tshouldAim = true;");
                    sw.WriteLine("\t\t\t}");
                    sw.WriteLine("\t\t}");
                    sw.WriteLine("\t}");
                    sw.WriteLine("");
                    sw.WriteLine("\tif (shouldAim) {");
                    
                    // skip the original two lines
                    i += 1;
                    
                    found = true;
                } else {
                    sw.WriteLine(lines[i]);
                }
            }
        }
        if (found) Console.WriteLine("Replaced successfully!");
        else Console.WriteLine("Not found!");
    }
}
