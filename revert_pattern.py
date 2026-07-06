import sys

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

old_parse = '''    motion = {
        "pattern": "ORBIT", # 何も指定がない場合でも、綺麗なループ形状（ORBIT）をデフォルトにする
        "vertical": "NONE",
        "turn_sign": 1.0,
        "loop": True,  # 敵のルートが必ずループできるように修正
        "speed_multiplier": 1.0,
        "amplitude_multiplier": 1.0,
        "opener": None,
        "keep_formation": False,
        "respect_bounds": True,
        "edge_margin": 2.0,
    }'''

new_parse = '''    motion = {
        "pattern": "DEFAULT",
        "vertical": "NONE",
        "turn_sign": 1.0,
        "loop": True,  # 敵のルートが必ずループできるように修正
        "speed_multiplier": 1.0,
        "amplitude_multiplier": 1.0,
        "opener": None,
        "keep_formation": False,
        "respect_bounds": True,
        "edge_margin": 2.0,
    }'''

text = text.replace(old_parse, new_parse)

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)
