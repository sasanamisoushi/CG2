using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string content = File.ReadAllText(path);

        string search = @"			// Y軸成分が大きい場合は天井・床
			if (std::abs(normal.y) > 0.9f) {
				if (normal.y > 0.0f) {
					rotate = { 1.570796f, 0.0f, 0.0f }; // 床: 面を+Yに向かせる
				} else {
					rotate = { -1.570796f, 3.14159265f, 0.0f }; // 天井: 面を-Yに向かせつつ、上下の向きを揃える
				}
			}";

        string replace = @"			// Y軸成分が大きい場合は天井・床
			if (std::abs(normal.y) > 0.9f) {
				if (normal.y > 0.0f) {
					rotate = { 1.570796f, 0.0f, 0.0f }; // 床: 面を+Yに向かせる
				} else {
					rotate = { -1.570796f, 0.0f, 0.0f }; // 天井: 反転を防ぐためY回転はしない
				}
			}";

        content = content.Replace(search, replace);
        
        string scaleSearch = "boundaryAlertObject_->SetScale({ 2.0f, 1.0f, 1.0f });";
        string scaleReplace = "boundaryAlertObject_->SetScale({ 2.0f, 2.0f, 2.0f });";
        content = content.Replace(scaleSearch, scaleReplace);

        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done");
    }
}
