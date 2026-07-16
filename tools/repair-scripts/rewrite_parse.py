import sys
import re

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

new_func = '''def _parse_ai_motion_prompt(prompt, style):
    normalized = str(prompt or "").casefold()
    motion = {
        "pattern": "DEFAULT",
        "vertical": "NONE",
        "turn_sign": 1.0,
        "loop": True,
        "speed_multiplier": 1.0,
        "amplitude_multiplier": 1.0,
        "opener": None,
        "keep_formation": False,
        "respect_bounds": True,
        "edge_margin": 2.0,
    }

    if _prompt_has(normalized, "反時計", "左回り", "counter", "ccw"):
        motion["turn_sign"] = -1.0
    elif _prompt_has(normalized, "時計", "右回り", "clockwise", "cw"):
        motion["turn_sign"] = 1.0

    wants_edge = _prompt_has(normalized, "フィールド", "外周", "端", "端沿い", "ぎりぎり", "ギリギリ", "境界", "枠", "場外", "edge", "border", "boundary")
    wants_orbit = _prompt_has(normalized, "回る", "旋回", "円", "一周", "一週", "1周", "1週", "周回", "巡回", "ループ", "orbit", "circle", "loop")
    if _prompt_has(normalized, "群れ", "隊列", "編隊", "フォーメーション", "まとま", "崩さ", "一団", "formation", "swarm"):
        motion["keep_formation"] = True
        motion["opener"] = "ALL"
    if _prompt_has(normalized, "外に出ない", "外へ出ない", "はみ出", "出ない", "収め", "範囲内", "inside", "bounds"):
        motion["respect_bounds"] = True
    if _prompt_has(normalized, "ぎりぎり", "ギリギリ", "端沿い", "外周", "境界", "edge", "border"):
        motion["edge_margin"] = 1.0

    if _prompt_has(normalized, "板野サーカス", "マクロス", "macross"):
        motion["pattern"] = "SPIRAL"
        motion["speed_multiplier"] = 1.8
        motion["amplitude_multiplier"] = 1.5
    elif _prompt_has(normalized, "ファンネル", "ドラグーン", "funnel", "dragoon"):
        motion["pattern"] = "ORBIT"
        motion["keep_formation"] = False
        motion["opener"] = "ALL"
        motion["speed_multiplier"] = 1.5
        motion["amplitude_multiplier"] = 1.3
    elif _prompt_has(normalized, "トランザム", "ゼロシステム", "trans-am", "transam"):
        motion["pattern"] = "ZIGZAG"
        motion["speed_multiplier"] = 2.0
        motion["amplitude_multiplier"] = 1.4
    elif _prompt_has(normalized, "超スピード", "神速", "超光速"):
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

    if _prompt_has(normalized, "上昇", "上に", "上が", "登", "高く", "climb", "up"):
        motion["vertical"] = "UP"
    elif _prompt_has(normalized, "下降", "下に", "下が", "降下", "低く", "dive", "down"):
        motion["vertical"] = "DOWN"
    elif _prompt_has(normalized, "上下", "波", "ウェーブ", "wave", "up and down"):
        motion["vertical"] = "WAVE"
    elif _prompt_has(normalized, "急降下", "ダイブ", "dive", "drop"):
        motion["vertical"] = "DIVE"

    if _prompt_has(normalized, "速", "猛", "ダッシュ", "fast", "speed", "quick", "rapid") and motion["speed_multiplier"] == 1.0:
        motion["speed_multiplier"] = 1.6
    elif _prompt_has(normalized, "遅", "ゆっくり", "緩やか", "slow"):
        motion["speed_multiplier"] = 0.5
        
    return motion'''

pattern = r'def _parse_ai_motion_prompt\(prompt, style\):.*?return motion'
text = re.sub(pattern, new_func, text, flags=re.DOTALL)

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)
