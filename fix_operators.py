import sys

op_file = r"project\resources\level_editor\operators.py"
with open(op_file, "r", encoding="utf-8") as f:
    lines = f.readlines()

new_lines = []
in_motion = False
for line in lines:
    if line.startswith("def _parse_ai_motion_prompt"):
        in_motion = True
        new_lines.append(line)
        continue
    
    if in_motion and line.startswith("def "):
        in_motion = False
        
    if in_motion and "_prompt_has(normalized" in line:
        if "counter" in line:
            new_lines.append('    if _prompt_has(normalized, "反時計回り", "左回り", "counter", "ccw"):\n')
        elif "clockwise" in line:
            new_lines.append('    elif _prompt_has(normalized, "時計回り", "右回り", "clockwise", "cw"):\n')
        elif "edge" in line and "wants_edge =" in line:
            new_lines.append('    wants_edge = _prompt_has(normalized, "端", "壁", "縁", "周り", "外周", "縁沿い", "境界", "フチ", "外側", "edge", "border", "boundary")\n')
        elif "orbit" in line and "wants_orbit =" in line:
            new_lines.append('    wants_orbit = _prompt_has(normalized, "回る", "周回", "円", "ループ", "ぐるぐる", "円を描く", "orbit", "circle", "loop")\n')
        elif "formation" in line:
            new_lines.append('    if _prompt_has(normalized, "隊列", "集団", "群れ", "フォーメーション", "まとまる", "swarm", "formation"):\n')
        elif "inside" in line:
            new_lines.append('    if _prompt_has(normalized, "壁に沿って", "端に沿って", "内側", "枠内", "沿う", "境界", "inside", "bounds"):\n')
        elif "wait" in line:
            new_lines.append('    if _prompt_has(normalized, "待機", "止まる", "静止", "動かない", "その場", "wait", "stay", "stop", "idle"):\n')
        elif "pingpong" in line:
            new_lines.append('    elif _prompt_has(normalized, "往復", "行ったり来たり", "パトロール", "巡回", "pingpong", "patrol"):\n')
        elif "teleport" in line:
            new_lines.append('    elif _prompt_has(normalized, "瞬間移動", "ワープ", "テレポート", "teleport", "warp"):\n')
        elif "random" in line:
            new_lines.append('    elif _prompt_has(normalized, "ランダム", "ランダムウォーク", "不規則", "random", "wander"):\n')
        elif "dash" in line:
            new_lines.append('    elif _prompt_has(normalized, "突撃", "突進", "ダッシュ", "高速", "dash", "charge", "rush"):\n')
        elif "transam" in line:
            new_lines.append('    elif _prompt_has(normalized, "トランザム", "transam"):\n')
        elif "speed_multiplier" in line and "2.5" in lines[lines.index(line)+1]:
            new_lines.append('    elif _prompt_has(normalized, "超スピード", "神速", "超高速"):\n')
        elif "spiral" in line:
            new_lines.append('    elif _prompt_has(normalized, "螺旋", "スパイラル", "spiral"):\n')
        elif "zigzag" in line:
            new_lines.append('    elif _prompt_has(normalized, "ジグザグ", "蛇行", "左右", "s字", "s-字", "zigzag", "zig-zag"):\n')
        elif "straight" in line:
            new_lines.append('    elif _prompt_has(normalized, "突撃", "直線", "まっすぐ", "正面", "接近", "rush", "charge", "straight"):\n')
        elif "up" in line and "climb" in line:
            new_lines.append('    if _prompt_has(normalized, "上昇", "上に", "上が", "昇", "高く", "climb", "up"):\n')
        elif "down" in line and "dive" in line:
            new_lines.append('    elif _prompt_has(normalized, "下降", "下に", "下が", "降下", "低く", "dive", "down"):\n')
        elif "wave" in line:
            new_lines.append('    elif _prompt_has(normalized, "上下", "波", "ウェーブ", "wave", "up and down"):\n')
        elif "drop" in line:
            new_lines.append('    elif _prompt_has(normalized, "急降下", "ダイブ", "dive", "drop"):\n')
        elif "fast" in line:
            new_lines.append('    if _prompt_has(normalized, "速", "急", "ダッシュ", "fast", "speed", "quick", "rapid") and motion["speed_multiplier"] == 1.0:\n')
        elif "slow" in line:
            new_lines.append('    elif _prompt_has(normalized, "遅", "ゆっくり", "緩やか", "slow"):\n')
        else:
            new_lines.append(line)
    else:
        new_lines.append(line)

with open(op_file, "w", encoding="utf-8") as f:
    f.writelines(new_lines)
print("done")
