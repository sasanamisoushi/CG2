import sys

op_file = r"project\resources\level_editor\operators.py"
with open(op_file, "r", encoding="utf-8") as f:
    text = f.read()

# I will find the start and end of _parse_ai_motion_prompt
import re

start_idx = text.find("def _parse_ai_motion_prompt")
end_idx = text.find("def _parse_ai_level_prompt", start_idx)
if start_idx != -1 and end_idx != -1:
    new_motion = """def _parse_ai_motion_prompt(prompt, style='BALANCED'):
    normalized = str(prompt or "").casefold()
    motion = {
        "pattern": "STAY" if style == 'STATIC' else "PINGPONG" if style == 'PATROL' else "RANDOM",
        "vertical": "NONE",
        "turn_sign": 1.0,
        "speed_multiplier": 1.0,
        "amplitude_multiplier": 1.0,
        "opener": None,
        "keep_formation": False,
        "respect_bounds": True,
        "edge_margin": 2.0,
    }

    if _prompt_has(normalized, "反時計回り", "左回り", "counter", "ccw"):
        motion["turn_sign"] = -1.0
    elif _prompt_has(normalized, "時計回り", "右回り", "clockwise", "cw"):
        motion["turn_sign"] = 1.0

    wants_edge = _prompt_has(normalized, "端", "壁", "縁", "周り", "外周", "縁沿い", "境界", "フチ", "外側", "edge", "border", "boundary")
    wants_orbit = _prompt_has(normalized, "回る", "周回", "円", "ループ", "ぐるぐる", "円を描く", "orbit", "circle", "loop")
    if _prompt_has(normalized, "隊列", "集団", "群れ", "フォーメーション", "まとまる", "swarm", "formation"):
        motion["keep_formation"] = True
        motion["opener"] = "ALL"
    if _prompt_has(normalized, "壁に沿って", "端に沿って", "内側", "枠内", "沿う", "境界", "inside", "bounds"):
        motion["respect_bounds"] = True
        
    if _prompt_has(normalized, "待機", "止まる", "静止", "動かない", "その場", "wait", "stay", "stop", "idle"):
        motion["pattern"] = "STAY"
    elif _prompt_has(normalized, "往復", "行ったり来たり", "パトロール", "巡回", "pingpong", "patrol"):
        motion["pattern"] = "PINGPONG"
    elif _prompt_has(normalized, "瞬間移動", "ワープ", "テレポート", "teleport", "warp"):
        motion["pattern"] = "TELEPORT"
    elif _prompt_has(normalized, "ランダム", "ランダムウォーク", "不規則", "random", "wander"):
        motion["pattern"] = "RANDOM"
    elif _prompt_has(normalized, "突撃", "突進", "ダッシュ", "高速", "dash", "charge", "rush"):
        motion["pattern"] = "DASH"
        motion["speed_multiplier"] = 2.0
    elif _prompt_has(normalized, "トランザム", "transam"):
        motion["pattern"] = "ZIGZAG"
        motion["speed_multiplier"] = 2.0
        motion["amplitude_multiplier"] = 1.4
    elif _prompt_has(normalized, "超スピード", "神速", "超高速"):
        motion["speed_multiplier"] = 2.5

    if wants_edge and wants_orbit:
        motion["pattern"] = "EDGE_ORBIT"
    elif _prompt_has(normalized, "螺旋", "スパイラル", "spiral"):
        motion["pattern"] = "SPIRAL"
        motion["amplitude_multiplier"] = max(motion["amplitude_multiplier"], 1.15)
    elif _prompt_has(normalized, "ジグザグ", "蛇行", "左右", "s字", "s-字", "zigzag", "zig-zag"):
        motion["pattern"] = "ZIGZAG"
        motion["amplitude_multiplier"] = max(motion["amplitude_multiplier"], 1.25)
    elif wants_orbit:
        motion["pattern"] = "ORBIT"
    elif _prompt_has(normalized, "突撃", "直線", "まっすぐ", "正面", "接近", "rush", "charge", "straight"):
        motion["pattern"] = "STRAIGHT"
        motion["loop"] = False

    if _prompt_has(normalized, "上昇", "上に", "上が", "昇", "高く", "climb", "up"):
        motion["vertical"] = "UP"
    elif _prompt_has(normalized, "下降", "下に", "下が", "降下", "低く", "dive", "down"):
        motion["vertical"] = "DOWN"
    elif _prompt_has(normalized, "上下", "波", "ウェーブ", "wave", "up and down"):
        motion["vertical"] = "WAVE"
    elif _prompt_has(normalized, "急降下", "ダイブ", "dive", "drop"):
        motion["vertical"] = "DIVE"

    if _prompt_has(normalized, "速", "急", "ダッシュ", "fast", "speed", "quick", "rapid") and motion["speed_multiplier"] == 1.0:
        motion["speed_multiplier"] = 1.6
    elif _prompt_has(normalized, "遅", "ゆっくり", "緩やか", "slow"):
        motion["speed_multiplier"] = 0.5
        
    return motion


"""
    text = text[:start_idx] + new_motion + text[end_idx:]

with open(op_file, "w", encoding="utf-8") as f:
    f.write(text)
print("done")
