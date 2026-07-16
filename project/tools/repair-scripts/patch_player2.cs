using System;
using System.IO;
using System.Text.RegularExpressions;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\Player.cpp";
        string content = File.ReadAllText(path);

        string search = @"void Player::OnCollision\(\) \{\s*isDead_ = true;\s*if \(object_\) \{\s*object_\.reset\(\);\s*\}\s*\}";
        string replace = @"void Player::OnCollision() {
	isDead_ = true;
	if (object_) {
		object_.reset();
	}
	if (boosterEffect_) {
		boosterEffect_.reset();
	}
}";
        
        content = Regex.Replace(content, search, replace);
        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done replacing OnCollision");
    }
}
