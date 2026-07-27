import re

# 1. Fix BoosterEffect.cpp
booster_path = r"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\BoosterEffect.cpp"
with open(booster_path, "r", encoding="utf-8") as f:
    text = f.read()

# Fix the huge scale multiplication
update_pattern = re.compile(r"(float targetScaleZ = ).*?;(.*?float noise = std::sin\(time_\) \* 0\.08f \+ \(float\)\(rand\(\) % 8\) \* 0\.01f;.*?burner\.currentScale\.x \+=).*?;(.*?)", re.MULTILINE | re.DOTALL)

def fix_booster_scale(match):
    # Basically we cap speedRatio
    # original: float targetScaleZ = speedRatio * 1.5f + (isAccelerating ? 0.8f : 0.2f);
    return r"""float cappedSpeedRatio = std::min(speedRatio, 1.5f);
    float targetScaleZ = cappedSpeedRatio * 1.2f + (isAccelerating ? 1.5f : 0.2f);
    float noise = std::sin(time_) * 0.08f + (float)(rand() % 8) * 0.01f;
    targetScaleZ += noise;
    if (targetScaleZ < 0.1f) targetScaleZ = 0.1f;

    float targetAlpha = cappedSpeedRatio * 0.6f + (isAccelerating ? 0.4f : 0.15f);
    targetAlpha = std::clamp(targetAlpha + noise * 0.2f, 0.0f, 1.0f);

    Matrix4x4 rotationMatrix = MyMath::MakeRotateMatrix(rotation);

    for (auto& burner : burners_) {
        burner.currentScale.z += (burner.defaultScale.z * targetScaleZ - burner.currentScale.z) * 0.3f;
        // X, Y scale shouldn't increase drastically
        float targetXY = burner.defaultScale.x * (isAccelerating ? 1.5f : 0.8f);
        burner.currentScale.x += (targetXY - burner.currentScale.x) * 0.3f;
        burner.currentScale.y += (targetXY - burner.currentScale.y) * 0.3f;
"""

# Actually, let's just do an exact string replace since we know the contents
broken_booster_block = """    // 速度や入力に応じたスケール変更
    float targetScaleZ = speedRatio * 1.5f + (isAccelerating ? 0.8f : 0.2f);
    // ゆらぎを追加
    float noise = std::sin(time_) * 0.08f + (float)(rand() % 8) * 0.01f;
    targetScaleZ += noise;
    if (targetScaleZ < 0.1f) targetScaleZ = 0.1f;

    // アルファ値も速度に連動
    float targetAlpha = speedRatio * 0.6f + (isAccelerating ? 0.4f : 0.15f);
    targetAlpha = std::clamp(targetAlpha + noise * 0.2f, 0.0f, 1.0f);

    Matrix4x4 rotationMatrix = MyMath::MakeRotateMatrix(rotation);

    for (auto& burner : burners_) {
        // 現在スケールをターゲットに補間
        burner.currentScale.z += (burner.defaultScale.z * targetScaleZ - burner.currentScale.z) * 0.3f;
        burner.currentScale.x += (burner.defaultScale.x * (speedRatio * 0.5f + 0.8f) - burner.currentScale.x) * 0.3f;
        burner.currentScale.y += (burner.defaultScale.y * (speedRatio * 0.5f + 0.8f) - burner.currentScale.y) * 0.3f;"""

fixed_booster_block = """    // 速度や入力に応じたスケール変更
    float cappedSpeedRatio = std::min(speedRatio, 1.2f); // クランプして巨大化を防ぐ
    float targetScaleZ = cappedSpeedRatio * 1.5f + (isAccelerating ? 2.5f : 0.2f); // ブースト時は少し長く
    
    // ゆらぎを追加
    float noise = std::sin(time_) * 0.08f + (float)(rand() % 8) * 0.01f;
    targetScaleZ += noise;
    if (targetScaleZ < 0.1f) targetScaleZ = 0.1f;

    // アルファ値も速度に連動
    float targetAlpha = cappedSpeedRatio * 0.6f + (isAccelerating ? 0.8f : 0.15f);
    targetAlpha = std::clamp(targetAlpha + noise * 0.2f, 0.0f, 1.0f);

    Matrix4x4 rotationMatrix = MyMath::MakeRotateMatrix(rotation);

    for (auto& burner : burners_) {
        // 現在スケールをターゲットに補間
        burner.currentScale.z += (burner.defaultScale.z * targetScaleZ - burner.currentScale.z) * 0.3f;
        
        // 太さ(X, Y)は速度に依存させず、一定の太さに抑える（黒い巨大な塊になるのを防ぐ）
        float targetXY = burner.defaultScale.x * (isAccelerating ? 1.2f : 0.8f);
        burner.currentScale.x += (targetXY - burner.currentScale.x) * 0.3f;
        burner.currentScale.y += (targetXY - burner.currentScale.y) * 0.3f;"""

if broken_booster_block in text:
    text = text.replace(broken_booster_block, fixed_booster_block)
    print("Fixed BoosterEffect scale logic!")
else:
    print("Could not find BoosterEffect broken block.")
    
# Let's fix the model rendering black by disabling lighting for it, or just relying on the scale fix.
# Actually, the user asked for an "effect" when boosting because currently it's just speed.
# I can add Trail to Player.cpp
with open(booster_path, "w", encoding="utf-8") as f:
    f.write(text)
