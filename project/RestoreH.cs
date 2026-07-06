using System;
using System.IO;
using System.Text.RegularExpressions;
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        string logFile = @"C:\Users\k024g\.gemini\antigravity\brain\3c48e713-9788-4824-9c17-4b18d6b66579\.system_generated\logs\transcript_full.jsonl";
        string targetFile = "GamePlayScene.h";
        
        foreach (string line in File.ReadLines(logFile))
        {
            if (line.Contains("\"type\":\"VIEW_FILE\"") && line.Contains(targetFile) && line.Contains("The above content shows the entire, complete file contents"))
            {
                Match contentMatch = Regex.Match(line, "\"content\":\"(.*?)\"}", RegexOptions.Singleline);
                if (contentMatch.Success)
                {
                    string content = contentMatch.Groups[1].Value.Replace("\\r\\n", "\n").Replace("\\n", "\n").Replace("\\t", "\t");
                    string[] lines = content.Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);
                    List<string> codeLines = new List<string>();
                    
                    foreach (string l in lines)
                    {
                        int colonIndex = l.IndexOf(": ");
                        if (colonIndex > 0)
                        {
                            string prefix = l.Substring(0, colonIndex);
                            int lineNum;
                            if (int.TryParse(prefix, out lineNum))
                            {
                                codeLines.Add(l.Substring(colonIndex + 2).TrimEnd('\r'));
                            }
                        }
                    }
                    
                    File.WriteAllLines(@"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.h", codeLines);
                    Console.WriteLine("Successfully restored GamePlayScene.h");
                    return;
                }
            }
        }
        Console.WriteLine("Could not find full GamePlayScene.h in logs");
    }
}
