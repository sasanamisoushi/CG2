import sys
import re

op_file = r"project\resources\level_editor\operators.py"
with open(op_file, "r", encoding="utf-8") as f:
    text = f.read()

# The correct versions:
coerce_correct = """def _coerce_ai_point(value):
    import mathutils
    if isinstance(value, dict) and "x" in value and "y" in value and "z" in value:
        return mathutils.Vector((float(value["x"]), float(value["y"]), float(value["z"])))
    if isinstance(value, list) and len(value) >= 3:
        return mathutils.Vector((float(value[0]), float(value[1]), float(value[2])))
    return None"""

sanitize_correct = """def _sanitize_enemy_plan_data(plan_data, count, center, extents, player):
    import bpy
    import mathutils
    from mathutils import Vector
    scene = bpy.context.scene
    enemies = plan_data.get("enemies") if isinstance(plan_data, dict) else None
    if not isinstance(enemies, list) or not enemies:
        raise ValueError("Geminiから敵データが返ってきませんでした。")

    blueprints = []
    margin = Vector((1.0, 1.0, 0.8))
    for index, enemy in enumerate(enemies[:count]):
        if not isinstance(enemy, dict):
            continue

        raw_path = enemy.get("path", [])
        points = []
        if isinstance(raw_path, list):
            for raw_point in raw_path[:12]:
                point = _coerce_ai_point(raw_point)
                if point is not None:
                    point = _clamp_point_to_box(point, center, extents, margin)
                    point = _push_out_of_obstacles(point, scene)
                    points.append(point)

        spawn = _coerce_ai_point(enemy.get("spawn"))
        if spawn is None and points:
            spawn = points[0]
        if spawn is None:
            continue
            
        spawn = _clamp_point_to_box(spawn, center, extents, margin)
        spawn = _push_out_of_obstacles(spawn, scene)

        if points:
            if (points[0] - spawn).length > 0.05:
                points.insert(0, spawn)
            else:
                points[0] = spawn
        else:
            points = [spawn]

        points = _sanitize_path_distances(points)
        if len(points) < 2 or all((point - spawn).length < 0.05 for point in points[1:]):
            offset = Vector((1.0 if spawn.x < center.x else -1.0, 0, 0))
            points = [spawn, _clamp_point_to_box(spawn + offset, center, extents, margin)]

        loop = bool(enemy.get("loop", False))
        speed = float(enemy.get("speed", 0.05))
        if speed <= 0 or speed > 0.5:
            speed = 0.05
            
        handle_type = str(enemy.get("handle_type", "AUTO")).upper()
        if handle_type not in {'AUTO', 'VECTOR', 'FREE', 'ALIGNED'}:
            handle_type = 'AUTO'

        blueprints.append({
            "spawn": spawn,
            "path": points,
            "loop": loop,
            "speed": speed,
            "handle_type": handle_type
        })
        
    return blueprints"""

# Remove all existing def _coerce_ai_point blocks
text = re.sub(r'def _coerce_ai_point\(value\):.*?(?=\n\w|\Z)', '', text, flags=re.DOTALL)
# Remove all existing def _sanitize_gemini_enemy_plan blocks
text = re.sub(r'def _sanitize_gemini_enemy_plan\(plan_data, count, center, extents, player\):.*?(?=\n\w|\Z)', '', text, flags=re.DOTALL)
# Remove any def _sanitize_enemy_plan_data if it somehow exists
text = re.sub(r'def _sanitize_enemy_plan_data\(plan_data, count, center, extents, player\):.*?(?=\n\w|\Z)', '', text, flags=re.DOTALL)

# Also rename calls to _sanitize_gemini_enemy_plan to _sanitize_enemy_plan_data
text = text.replace('_sanitize_gemini_enemy_plan', '_sanitize_enemy_plan_data')

# Inject them at the top after def _stage_bounds_prompt
if "def _stage_bounds_prompt(" in text:
    parts = text.split("def _stage_bounds_prompt(", 1)
    text = parts[0] + coerce_correct + "\n\n" + sanitize_correct + "\n\ndef _stage_bounds_prompt(" + parts[1]
else:
    print("Could not find _stage_bounds_prompt!")

with open(op_file, "w", encoding="utf-8") as f:
    f.write(text)
print("Cleaned up and updated operators.py successfully!")
