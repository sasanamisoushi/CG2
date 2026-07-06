using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string content = File.ReadAllText(path);

        string searchSpawnComment = @"// 	SpawnEnemiesFromSpawnPoints();";
        string replaceSpawnLogic = @"	for (size_t i = 0; i < enemySpawns_.size(); ++i) {
		if (enemySpawns_[i].isInitialSpawn) {
			auto enemy = std::make_unique<Enemy>();
			enemy->Initialize(enemySpawns_[i].position);
			enemy->SetRotation(enemySpawns_[i].rotation);
			enemy->SetSpawnPointIndex(i);
			enemies_.push_back(std::move(enemy));
		}
	}";
        content = content.Replace(searchSpawnComment, replaceSpawnLogic);

        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done replacing spawn comments");
    }
}
