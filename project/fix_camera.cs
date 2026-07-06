using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string content = File.ReadAllText(path);

        // 1. Add enemy->Update(...) to GamePlayScene::Update
        string updateEnemySearch = @"		for (auto it = enemies_.begin(); it != enemies_.end(); ) {
			(*it)->Update(playerPos, enemyBulletManager_.get(), obstacles_);";
        if (!content.Contains(updateEnemySearch)) {
            // Already there or differently formatted, let's just make sure we are not duplicating.
            // Oh wait, my grep showed it IS there inside the 'if (updateSelectedEnemies)' block!
        }
        
        // Let's fix the camera update bug by moving camera->Update OUT of the else block!
        string cameraUpdateSearch = @"		// 障害物自身のUpdateを回す（現状中身は空に近いですが一応回します）
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	} else {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
	}";
        string cameraUpdateReplace = @"		// 障害物自身のUpdateを回す（現状中身は空に近いですが一応回します）
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	} else {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
	}

	// 追従カメラの更新はSimulationかどうかに限らず、デバッグカメラでなければ行う
	if (!isDebugCameraActive_) {
		if (player_) {
			Vector3* targetPos = nullptr;
			Vector3 enemyPos;
			if (lockedEnemy_) {
				enemyPos = lockedEnemy_->GetPosition();
				targetPos = &enemyPos;
			}
			player_->UpdateCamera(camera.get(), targetPos);
		}
		camera->Update();
		if (skybox) skybox->Update(camera.get());
	}";

        content = content.Replace(cameraUpdateSearch, cameraUpdateReplace);
        
        // Remove the duplicated camera update from the end of GamePlayScene::Update
        string duplicateCameraSearch = @"	} else {
		if (player_) {
			Vector3* targetPos = nullptr;
			Vector3 enemyPos;
			if (lockedEnemy_) {
				enemyPos = lockedEnemy_->GetPosition();
				targetPos = &enemyPos;
			}
			player_->UpdateCamera(camera.get(), targetPos);
		}
		camera->Update();
		skybox->Update(camera.get());
	}";
        string duplicateCameraReplace = @"	}";
        content = content.Replace(duplicateCameraSearch, duplicateCameraReplace);

        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done Camera Fix");
    }
}
