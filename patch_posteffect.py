import os

file_path = r"project\Game\Scene\GamePlayScene.cpp"

with open(file_path, "r", encoding="utf-8") as f:
    content = f.read()

target = """	} else {
		// 騾壼ｸｸ譎ゑｼ壹ヮ繝ｼ繝槭Ν繧ｨ繝輔ぉ繧ｯ繝医€√∪縺溘・繝悶・繧ｹ繝域凾縺ｮ繧ｹ繝斐・繝画ｼ泌・
		if (PostEffect::GetInstance()) {
			bool isBoosting = false;"""

if target in content:
    replacement = """	} else {
		// 騾壼ｸｸ譎ゑｼ壹ヮ繝ｼ繝槭Ν繧ｨ繝輔ぉ繧ｯ繝医€√∪縺溘・繝悶・繧ｹ繝域凾縺ｮ繧ｹ繝斐・繝画ｼ泌・
		if (PostEffect::GetInstance()) {
			if (player_) {
				if (previousPlayerHP_ == -1) {
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() < previousPlayerHP_) {
					damageEffectTimer_ = 30; // 0.5s effect
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() > previousPlayerHP_) {
					previousPlayerHP_ = player_->GetHP();
				}
			}

			if (damageEffectTimer_ > 0) {
				damageEffectTimer_--;
				PostEffect::GetInstance()->SetEffectType(13); // Fold Wave
			} else {
				bool isBoosting = false;"""
    content = content.replace(target, replacement)
    
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(content)
    print("Patched successfully")
else:
    print("Target not found, trying with different line endings or partial match")
    lines = content.split('\n')
    for i, line in enumerate(lines):
        if "bool isBoosting = false;" in line and "PostEffect::GetInstance()" in lines[i-1]:
            lines.insert(i, """			if (player_) {
				if (previousPlayerHP_ == -1) {
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() < previousPlayerHP_) {
					damageEffectTimer_ = 30; // 0.5s effect
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() > previousPlayerHP_) {
					previousPlayerHP_ = player_->GetHP();
				}
			}

			if (damageEffectTimer_ > 0) {
				damageEffectTimer_--;
				PostEffect::GetInstance()->SetEffectType(13); // Fold Wave
			} else {""")
            for j in range(i+1, len(lines)):
                if "if (!isBoosting) {" in lines[j]:
                    lines.insert(j+3, "\t\t\t}")
                    break
            content = '\n'.join(lines)
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(content)
            print("Patched successfully using fallback")
            break
