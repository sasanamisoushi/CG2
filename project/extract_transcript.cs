using System;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;
using System.Collections.Generic;

class Program {
    static void Main() {
        string transcriptPath = @"C:\Users\k024g\.gemini\antigravity\brain\3c48e713-9788-4824-9c17-4b18d6b66579\.system_generated\logs\transcript_full.jsonl";
        string outputPath = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\GamePlayScene_Backup.cpp";
        
        string bestContent = null;
        JavaScriptSerializer jss = new JavaScriptSerializer();
        jss.MaxJsonLength = int.MaxValue;
        
        using (StreamReader sr = new StreamReader(transcriptPath, Encoding.UTF8)) {
            string line;
            while ((line = sr.ReadLine()) != null) {
                try {
                    Dictionary<string, object> dict = jss.Deserialize<Dictionary<string, object>>(line);
                    if (dict.ContainsKey("type") && dict["type"].ToString() == "TOOL_RESPONSE" &&
                        dict.ContainsKey("status") && dict["status"].ToString() == "DONE" &&
                        dict.ContainsKey("content")) {
                        string content = dict["content"].ToString();
                        if (content.Contains("void GamePlayScene::Initialize()") && 
                            content.Contains("GamePlayScene::Draw()") &&
                            content.Length > 100000) {
                            bestContent = content;
                        }
                    }
                } catch { }
            }
        }
        
        if (bestContent != null) {
            int idx = bestContent.IndexOf("#include \"GamePlayScene.h\"");
            if (idx != -1) bestContent = bestContent.Substring(idx);
            
            // Remove markdown ticks if any
            if (bestContent.EndsWith("`")) bestContent = bestContent.Substring(0, bestContent.Length - 3);
            
            File.WriteAllText(outputPath, bestContent, new UTF8Encoding(true));
            Console.WriteLine("Success! Saved backup of GamePlayScene.cpp with length: " + bestContent.Length);
        } else {
            Console.WriteLine("Could not find a valid full GamePlayScene.cpp in transcript.");
        }
    }
}
