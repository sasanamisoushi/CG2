using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\Player.cpp";
        string content = File.ReadAllText(path);

        string search = @"void Player::OnCollision() {
	isDead_ = true;
	if (object_) {
		object_.reset();
	}
}";
        string replace = @"void Player::OnCollision() {
	isDead_ = true;
	if (object_) {
		object_.reset();
	}
	if (boosterEffect_) {
		boosterEffect_.reset();
	}
}";
        if (content.Contains(search)) {
            content = content.Replace(search, replace);
            File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
            Console.WriteLine("Successfully patched OnCollision.");
        } else {
            Console.WriteLine("Search string not found!");
        }
    }
}
