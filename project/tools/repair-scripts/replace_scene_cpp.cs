using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string content = File.ReadAllText(path);

        string search = @"	if (player_ && player_->IsNearBoundary() && boundaryAlertObject_) {
		float intensity = player_->GetBoundaryWarningIntensity();
		Vector3 position = player_->GetBoundaryAlertPosition();
		Vector3 normal = player_->GetBoundaryAlertNormal();

		// プレイヤーと同じくらいのサイズにする（例: 2.0f ～ 3.0f）
		boundaryAlertObject_->SetScale({ 3.0f, 1.0f, 3.0f });

		// 横にならず縦に立たせるためのオイラー角回転
		// X軸で-90度回転して立たせ、Y軸で壁の法線方向を向かせる
		Vector3 rotate = { -1.570796f, std::atan2(normal.x, normal.z), 0.0f };

		static float pulseTime = 0.0f;
		pulseTime += 0.05f;
		float pulseAlpha = 0.5f + 0.5f * std::sin(pulseTime);
		
		Model* m = boundaryAlertObject_->GetModel();
		if (m) {
			m->SetColor({ 1.0f, 1.0f, 1.0f, intensity * pulseAlpha });
		}
		
		boundaryAlertObject_->SetTranslate({
			position.x + normal.x * 0.5f,
			position.y + normal.y * 0.5f,
			position.z + normal.z * 0.5f
		});
		
		boundaryAlertObject_->SetRotate(rotate);
		boundaryAlertObject_->Update();
		
		boundaryAlertObject_->Draw();
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
	}";

        string replace = @"	if (player_ && player_->IsNearBoundary() && boundaryAlertObject_) {
		static float pulseTime = 0.0f;
		pulseTime += 0.05f;
		float pulseAlpha = 0.5f + 0.5f * std::sin(pulseTime);

		for (const auto& alert : player_->GetBoundaryAlerts()) {
			float intensity = alert.intensity;
			Vector3 position = alert.position;
			Vector3 normal = alert.normal;

			boundaryAlertObject_->SetScale({ 3.0f, 1.0f, 3.0f });

			Vector3 rotate = { 0.0f, 0.0f, 0.0f };
			
			// Y軸成分が大きい場合は天井・床
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
			}
			
			Model* m = boundaryAlertObject_->GetModel();
			if (m) {
				m->SetColor({ 1.0f, 1.0f, 1.0f, intensity * pulseAlpha });
			}
			
			boundaryAlertObject_->SetTranslate({
				position.x + normal.x * 0.5f,
				position.y + normal.y * 0.5f,
				position.z + normal.z * 0.5f
			});
			
			boundaryAlertObject_->SetRotate(rotate);
			boundaryAlertObject_->Update();
			
			boundaryAlertObject_->Draw();
		}
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
	}";

        content = content.Replace(search, replace);
        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done");
    }
}
