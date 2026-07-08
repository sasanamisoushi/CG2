using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

class Program {
    static void Main() {
        string sceneFile = @"project\Game\Scene\GamePlayScene.cpp";
        string uiFile = @"project\Game\Scene\GamePlayUIManager.cpp";
        
        string sceneText = File.ReadAllText(sceneFile, new UTF8Encoding(false));
        
        // Match DrawGameplayActionControls
        var drawMatch = Regex.Match(sceneText, @"(?s)\nvoid GamePlayScene::DrawGameplayActionControls\(\) \{.*?\n\}\n");
        if (drawMatch.Success) {
            string drawText = drawMatch.Value;
            // Remove from sceneText
            sceneText = sceneText.Replace(drawText, "");
            
            // Modify for GamePlayUIManager
            string newDrawText = drawText.Replace("GamePlayScene::", "GamePlayUIManager::");
            newDrawText = newDrawText.Replace("simulationManager_->", "scene_->simulationManager_->");
            newDrawText = newDrawText.Replace("simulationActionNames_", "simulationActionNames_"); // No change needed, it's member
            
            // Append to GamePlayUIManager.cpp
            string uiText = File.ReadAllText(uiFile, new UTF8Encoding(false));
            uiText += "\r\n" + newDrawText.TrimStart('\n');
            File.WriteAllText(uiFile, uiText, new UTF8Encoding(false));
            Console.WriteLine("Moved DrawGameplayActionControls to GamePlayUIManager.cpp");
        } else {
            Console.WriteLine("DrawGameplayActionControls not found in GamePlayScene.cpp");
        }
        
        // Match UpdateUI
        var updateUiMatch = Regex.Match(sceneText, @"(?s)\nvoid GamePlayScene::UpdateUI\(\) \{.*?\n\}\n");
        if (updateUiMatch.Success) {
            sceneText = sceneText.Replace(updateUiMatch.Value, "");
            Console.WriteLine("Removed UpdateUI from GamePlayScene.cpp");
        } else {
            Console.WriteLine("UpdateUI not found in GamePlayScene.cpp");
        }
        
        // Update Update() call to UpdateUI()
        // Wait, the call is inside Update(). Let's find it.
        string oldCall = "#ifdef ENABLE_IMGUI\r\n\tUpdateUI();\r\n#endif";
        string newCall = "#ifdef ENABLE_IMGUI\r\n\tif (uiManager_) {\r\n\t\tif (environmentRenderer_) environmentRenderer_->Update(camera.get());\r\n\t\tuiManager_->UpdateUI();\r\n\t}\r\n#endif";
        
        if (sceneText.Contains(oldCall)) {
            sceneText = sceneText.Replace(oldCall, newCall);
            Console.WriteLine("Updated UpdateUI call in GamePlayScene::Update");
        } else {
            // It might use \n instead of \r\n
            oldCall = "#ifdef ENABLE_IMGUI\n\tUpdateUI();\n#endif";
            newCall = "#ifdef ENABLE_IMGUI\n\tif (uiManager_) {\n\t\tif (environmentRenderer_) environmentRenderer_->Update(camera.get());\n\t\tuiManager_->UpdateUI();\n\t}\n#endif";
            if (sceneText.Contains(oldCall)) {
                sceneText = sceneText.Replace(oldCall, newCall);
                Console.WriteLine("Updated UpdateUI call in GamePlayScene::Update (LF)");
            } else {
                Console.WriteLine("UpdateUI call not found in GamePlayScene::Update");
            }
        }
        
        File.WriteAllText(sceneFile, sceneText, new UTF8Encoding(false));
    }
}
