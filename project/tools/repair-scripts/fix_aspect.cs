using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.cpp";
        string content = File.ReadAllText(path);

        string searchAim = @"aimCursorSprite_->SetSize({ 64.0f, 64.0f });";
        string replaceAim = @"aimCursorSprite_->SetSize({ 64.0f * (2816.0f / 1536.0f), 64.0f });";

        string searchLock = @"lockOnReticleSprite_->SetSize({ reticleSize, reticleSize });";
        string replaceLock = @"lockOnReticleSprite_->SetSize({ reticleSize * (2816.0f / 1536.0f), reticleSize });";

        content = content.Replace(searchAim, replaceAim);
        content = content.Replace(searchLock, replaceLock);

        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Done aspect ratio fix");
    }
}
