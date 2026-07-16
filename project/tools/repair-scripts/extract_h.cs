using System;
using System.IO;
using System.Text.RegularExpressions;

class Program {
    static void Main() {
        string logPath = @"C:\Users\k024g\.gemini\antigravity\brain\3c48e713-9788-4824-9c17-4b18d6b66579\.system_generated\logs\transcript_full.jsonl";
        string[] lines = File.ReadAllLines(logPath);
        foreach (string line in lines) {
            if (line.Contains("\"type\":\"VIEW_FILE\"") && line.Contains("GamePlayScene.h")) {
                var match = Regex.Match(line, @"""content"":""(?:Created At[^""]*?|Completed At[^""]*?|The above content[^""]*?)((?:\\n|.)*?)""}");
                if (match.Success) {
                    string content = match.Groups[1].Value.Replace("\\n", "\n").Replace("\\r", "\r").Replace("\\\"", "\"").Replace("\\\\", "\\").Replace("\\t", "\t");
                    // Strip the 'The above content shows...' prefix
                    int idx = content.IndexOf("*/");
                    if (idx == -1) idx = content.IndexOf("#pragma");
                    if (idx != -1) {
                        content = content.Substring(idx);
                        File.WriteAllText(@"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.h", content, new System.Text.UTF8Encoding(true));
                        Console.WriteLine("Extracted GamePlayScene.h! Length: " + content.Length);
                        return;
                    }
                }
            }
        }
        Console.WriteLine("Could not find GamePlayScene.h in transcript.");
    }
}
