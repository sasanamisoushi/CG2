using System;
using System.IO;
using System.Text.RegularExpressions;
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        string cppFile = @"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string logFile = @"c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\build28.log";
        
        string[] lines = File.ReadAllLines(cppFile);
        HashSet<int> errorLines = new HashSet<int>();
        
        foreach (string logLine in File.ReadLines(logFile))
        {
            Match m = Regex.Match(logLine, @"GamePlayScene\.cpp\((\d+),\d+\): error C2001");
            if (m.Success)
            {
                errorLines.Add(int.Parse(m.Groups[1].Value) - 1); // 0-indexed
            }
        }
        
        foreach (int i in errorLines)
        {
            string line = lines[i];
            int firstQuote = line.IndexOf('"');
            if (firstQuote != -1)
            {
                // find the first comma AFTER the first quote
                int firstComma = line.IndexOf(',', firstQuote);
                if (firstComma != -1)
                {
                    lines[i] = line.Insert(firstComma, "\"");
                }
                else
                {
                    int endParen = line.LastIndexOf(')');
                    if (endParen != -1)
                    {
                        lines[i] = line.Insert(endParen, "\"");
                    }
                }
            }
            Console.WriteLine("Fixed line " + (i + 1) + ": " + lines[i]);
        }
        
        File.WriteAllLines(cppFile, lines);
    }
}
