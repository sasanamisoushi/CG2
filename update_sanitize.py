import sys

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

# Replace _sanitize_gemini_enemy_plan
old_sanitize = '''def _sanitize_gemini_enemy_plan(plan_data, count, center, extents, player):
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
                    points.append(_clamp_point_to_box(point, center, extents, margin))

        spawn = _coerce_ai_point(enemy.get("spawn"))
        if spawn is None and points:
            spawn = points[0]
        if spawn is None:
            continue
        spawn = _clamp_point_to_box(spawn, center, extents, margin)

        if points:
            if (points[0] - spawn).length > 0.05:
                points.insert(0, spawn)
            else:
                points[0] = spawn
        else:
            points = [spawn]

        if len(points) < 2 or all((point - spawn).length < 0.05 for point in points[1:]):
            points.append(_fallback_second_path_point(spawn, player, center, extents))'''

new_sanitize = '''def _sanitize_gemini_enemy_plan(plan_data, count, center, extents, player):
    import bpy
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
            points.append(_fallback_second_path_point(spawn, player, center, extents))'''

text = text.replace(old_sanitize, new_sanitize)

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)
