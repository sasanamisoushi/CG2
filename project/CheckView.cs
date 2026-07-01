using System;
using System.IO;
using System.Text.RegularExpressions;

class Program
{
    static void Main()
    {
        string logFile = @"C:\Users\k024g\.gemini\antigravity\brain\3c48e713-9788-4824-9c17-4b18d6b66579\.system_generated\logs\transcript_full.jsonl";
        foreach (string line in File.ReadLines(logFile))
        {
            if (line.Contains("\"type\":\"VIEW_FILE\"") && line.Contains("GamePlayScene.cpp"))
            {
                Match m = Regex.Match(line, "Total Lines: (\\d+)");
                Match m2 = Regex.Match(line, "Showing lines (\\d+) to (\\d+)");
                if (m.Success) {
                    Console.WriteLine("Viewed " + (m2.Success ? m2.Value : "all") + " out of " + m.Value);
                }
            }
        }
    }
}
