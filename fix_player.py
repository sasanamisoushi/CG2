import sys

op_path = r"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Player\Player.cpp"
with open(op_path, "r", encoding="utf-8") as f:
    text = f.read()

broken_block = """	// アクションアニメーションの自動ロード
	actionAnimations_.clear();
    modeParams_[2].moveDamping = 0.80f;"""

fixed_block = """	// アクションアニメーションの自動ロード
	actionAnimations_.clear();
	std::string animPath = "resources/animations";
	if (std::filesystem::exists(animPath)) {
		for (const auto& entry : std::filesystem::directory_iterator(animPath)) {
			if (entry.path().extension() == ".bana") {
				Animation anim;
				if (LoadAnimationFromBinary(anim, entry.path().string())) {
					actionAnimations_[entry.path().stem().string()] = anim;
				}
			}
		}
	}

    // 形態ごとの初期パラメータ設定
    // 0: Fighter
    modeParams_[0].maxMoveSpeed = 1.6f;
    modeParams_[0].moveAcceleration = 0.15f;
    modeParams_[0].moveDamping = 0.98f;
    modeParams_[0].pitchSpeed = 0.05f;
    modeParams_[0].yawSpeed = 0.045f;
    modeParams_[0].rollSpeed = 0.08f;

    // 1: Gerwalk
    modeParams_[1].maxMoveSpeed = 0.22f;
    modeParams_[1].moveAcceleration = 0.018f;
    modeParams_[1].moveDamping = 0.90f;
    modeParams_[1].pitchSpeed = 0.015f;
    modeParams_[1].yawSpeed = 0.014f;
    modeParams_[1].rollSpeed = 0.025f;

    // 2: Battroid
    modeParams_[2].maxMoveSpeed = 0.12f;
    modeParams_[2].moveAcceleration = 0.025f;
    modeParams_[2].moveDamping = 0.80f;"""

text = text.replace(broken_block, fixed_block)

with open(op_path, "w", encoding="utf-8") as f:
    f.write(text)

print("Fixed!")
