using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string content = File.ReadAllText(path);

        string search = @"	Vector3 fireDirection = forward;
	if (target && type == MissileType::MissileWithTrail) {
		Vector3 targetPosition = target->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = target->GetCollisionRadius();
		} catch (...) {}
		targetPosition.y += collisionRadius * 0.3f;
		fireDirection = NormalizeOrVector3(SubtractVector3(targetPosition, muzzlePos), forward);
	}";

        string replace = @"	Vector3 fireDirection = forward;
	bool shouldAim = false;
	if (target) {
		if (type == MissileType::MissileWithTrail) {
			shouldAim = true;
		} else if (type == MissileType::Normal) {
			PlayerMode mode = player_->GetCurrentMode();
			if (mode == PlayerMode::Gerwalk || mode == PlayerMode::Battroid) {
				shouldAim = true;
			}
		}
	}

	if (shouldAim) {
		Vector3 targetPosition = target->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = target->GetCollisionRadius();
		} catch (...) {}
		targetPosition.y += collisionRadius * 0.3f;
		fireDirection = NormalizeOrVector3(SubtractVector3(targetPosition, muzzlePos), forward);
	}";

        if (content.Contains(search)) {
            content = content.Replace(search, replace);
            File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
            Console.WriteLine("Successfully updated aiming logic.");
        } else {
            Console.WriteLine("Search string not found!");
        }
    }
}
