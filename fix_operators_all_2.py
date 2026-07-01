import sys

op_file = r"project\resources\level_editor\operators.py"
with open(op_file, "r", encoding="utf-8") as f:
    text = f.read()

import re

start_idx = text.find("def _parse_ai_level_prompt")
end_idx = text.find("def _build_ai_enemy_points", start_idx)
if start_idx != -1 and end_idx != -1:
    new_level = """def _parse_ai_level_prompt(prompt):
    normalized = str(prompt or "").casefold()
    plan = {
        "style": "CITY",
        "shape": "CUBE",
        "density_multiplier": 1.0,
        "is_modifier": False,
        "scale_modifiers": __import__('mathutils').Vector((1.0, 1.0, 1.0))
    }

    if _prompt_has(normalized, "小さく", "小さい", "縮小", "small", "shrink"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((0.5, 0.5, 0.5))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "大きく", "大きい", "拡大", "big", "large", "huge"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((2.0, 2.0, 2.0))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "高く", "高い", "tall", "height"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((1.0, 1.0, 2.0))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "低く", "低い", "short"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((1.0, 1.0, 0.5))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "長く", "長い", "long"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((2.0, 1.0, 1.0))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "平たく", "平ら", "flat"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((1.0, 1.0, 0.2))
        plan["is_modifier"] = True

    if _prompt_has(normalized, "迷路", "maze"):
        plan["style"] = "MAZE"
    elif _prompt_has(normalized, "防衛線", "防壁", "ライン", "defense", "line"):
        plan["style"] = "DEFENSE_LINE"
    elif _prompt_has(normalized, "アリーナ", "広場", "arena", "open"):
        plan["style"] = "ARENA"
    elif _prompt_has(normalized, "市街地", "街", "ビル", "city", "building"):
        plan["style"] = "CITY"
    elif _prompt_has(normalized, "ランダム", "大小", "さまざま", "色々", "random", "various"):
        plan["style"] = "RANDOM"

    if _prompt_has(normalized, "円柱", "柱", "cylinder", "pillar"):
        plan["shape"] = "CYLINDER"

    if _prompt_has(normalized, "たくさん", "大量", "多い", "many", "dense", "2倍", "２倍"):
        plan["density_multiplier"] = 2.0
    elif _prompt_has(normalized, "少し", "少ない", "まばら", "few", "sparse", "半分"):
        plan["density_multiplier"] = 0.5

    return plan


"""
    text = text[:start_idx] + new_level + text[end_idx:]

with open(op_file, "w", encoding="utf-8") as f:
    f.write(text)
print("done")
