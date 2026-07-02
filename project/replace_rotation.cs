using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string content = File.ReadAllText(path);

        string search = @"			// Y軸成分が大きい場合は天井・床
			if (std::abs(normal.y) > 0.9f) {
				if (normal.y > 0.0f) {
					rotate = { -1.570796f, 0.0f, 0.0f }; // 床
				} else {
					rotate = { 1.570796f, 0.0f, 0.0f }; // 天井
				}
			} else {
				// 縦の壁（XY平面に作られたモデルを法線方向へ向かせる）
				// InitializePlaneで作られる平面は初期状態で -Z 方向を向いている
				rotate = { 0.0f, std::atan2(-normal.x, -normal.z), 0.0f };
			}";

        string replace = @"			// Y軸成分が大きい場合は天井・床
			if (std::abs(normal.y) > 0.9f) {
				if (normal.y > 0.0f) {
					rotate = { 1.570796f, 0.0f, 0.0f }; // 床: 面を+Yに向かせる
				} else {
					rotate = { -1.570796f, 3.14159265f, 0.0f }; // 天井: 面を-Yに向かせつつ、上下の向きを揃える
				}
			} else {
				// 縦の壁（XY平面に作られたモデルを法線方向へ向かせる）
				// InitializePlaneで作られる平面は初期状態で -Z 方向を向いているので、法線が向くようにY軸回転
				rotate = { 0.0f, std::atan2(normal.x, -normal.z), 0.0f };
			}";

        content = content.Replace(search, replace);
        
        string scaleSearch = "boundaryAlertObject_->SetScale({ 3.0f, 1.0f, 3.0f });";
        string scaleReplace = "boundaryAlertObject_->SetScale({ 2.0f, 1.0f, 1.0f });";
        content = content.Replace(scaleSearch, scaleReplace);

        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done");
    }
}
