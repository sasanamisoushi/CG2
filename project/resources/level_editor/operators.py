import json
import math
import os
import random
import subprocess
import urllib.error
import urllib.parse
import urllib.request

import bpy
import bpy_extras
from bpy.app.handlers import persistent
from mathutils import Vector, geometry

import validation


_INVALID_FILENAME_CHARS = '<>:"/\\|?*'
_AI_GENERATED_MARKER = "myaddon_ai_generated"
_AI_GENERATED_COLLECTION = "AI Enemy Plan"


def _default_scene_json_path():
    blend_dir = os.path.dirname(bpy.data.filepath)
    if not blend_dir:
        return "scene.json"
    return os.path.join(blend_dir, "scene.json")


def _default_scene_obj_path():
    if not bpy.data.filepath:
        return "scene.obj"
    return os.path.splitext(bpy.data.filepath)[0] + ".obj"


def _workspace_root_from_blend():
    if bpy.data.filepath:
        return os.path.abspath(os.path.join(os.path.dirname(bpy.data.filepath), "..", ".."))
    return os.path.abspath(os.path.join(os.getcwd(), ".."))


def _default_game_exe_path():
    workspace_root = _workspace_root_from_blend()
    return os.path.join(workspace_root, "generated", "outputs", "Development", "CG2.exe")


def _default_game_working_dir():
    return os.path.join(_workspace_root_from_blend(), "project")


def _resolve_game_exe_path(scene):
    configured_path = getattr(scene, "myaddon_game_exe_path", "").strip()
    if configured_path:
        return os.path.abspath(bpy.path.abspath(configured_path))
    return _default_game_exe_path()


def _is_obj_export_target(obj):
    if obj.type != 'MESH':
        return False

    try:
        return obj.visible_get()
    except Exception:
        return not obj.hide_get()


def _is_individual_model_export_target(obj):
    if not _is_obj_export_target(obj):
        return False
    if obj.name.startswith("StageBounds"):
        return False

    category = getattr(obj, "game_obj_type", "NONE")
    return category not in {"PLAYER", "ENEMY"}


def _safe_filename_stem(name):
    stem = name.strip()
    for char in _INVALID_FILENAME_CHARS:
        stem = stem.replace(char, "_")
    stem = "".join("_" if ord(char) < 32 else char for char in stem)
    stem = stem.strip(" .")
    return stem or "Mesh"


def _build_model_filename_map(scene):
    model_filenames = {}
    used_filenames = set()

    for obj in scene.objects:
        if not _is_individual_model_export_target(obj):
            continue

        stem = _safe_filename_stem(obj.name)
        filename = f"{stem}.obj"
        index = 2
        while filename.lower() in used_filenames:
            filename = f"{stem}_{index}.obj"
            index += 1

        used_filenames.add(filename.lower())
        model_filenames[obj.name] = filename

    return model_filenames


def _get_world_transform(obj):
    trans, rot, scale = obj.matrix_world.decompose()

    # 敵モデルをリスポーン地点として使う場合は、原点ではなく見た目の中心を送る。
    if obj.type == "MESH" and getattr(obj, "game_obj_type", "NONE") == "ENEMY":
        local_center = sum((Vector(corner) for corner in obj.bound_box), Vector()) / 8.0
        trans = obj.matrix_world @ local_center

    return trans, rot, scale


def _is_enemy_path_object(obj):
    if obj.type != 'CURVE':
        return False
    path_id = getattr(obj, "enemy_path_id", "None")
    return path_id != "None" or obj.name.startswith("EnemyPath")


def _get_enemy_path_id(obj):
    path_id = getattr(obj, "enemy_path_id", "None")
    if path_id != "None":
        return path_id
    return obj.name.split(".", 1)[0]


def _get_curve_world_points(obj):
    points = []

    def append_world_point(local_point):
        world_point = obj.matrix_world @ local_point
        if points:
            previous = Vector(points[-1])
            if (world_point - previous).length <= 0.0001:
                return
        points.append([world_point.x, world_point.y, world_point.z])

    for spline in obj.data.splines:
        if spline.type == 'BEZIER':
            bezier_points = spline.bezier_points
            segment_count = len(bezier_points)
            if segment_count < 2:
                continue
            if not spline.use_cyclic_u:
                segment_count -= 1

            for index in range(segment_count):
                current = bezier_points[index]
                next_point = bezier_points[(index + 1) % len(bezier_points)]
                samples = geometry.interpolate_bezier(
                    current.co,
                    current.handle_right,
                    next_point.handle_left,
                    next_point.co,
                    max(2, obj.data.resolution_u),
                )
                for sample in samples:
                    append_world_point(sample)
        else:
            for point in spline.points:
                w = point.co.w if point.co.w != 0.0 else 1.0
                append_world_point(Vector((point.co.x / w, point.co.y / w, point.co.z / w)))

    return points


def _unique_enemy_path_id(scene):
    used_ids = {
        getattr(obj, "enemy_path_id", "None")
        for obj in scene.objects
        if getattr(obj, "enemy_path_id", "None") != "None"
    }

    if "EnemyPath" not in used_ids:
        return "EnemyPath"

    index = 1
    while True:
        candidate = f"EnemyPath{index:03d}"
        if candidate not in used_ids:
            return candidate
        index += 1


def _get_ai_collection(scene):
    collection = bpy.data.collections.get(_AI_GENERATED_COLLECTION)
    if collection is None:
        collection = bpy.data.collections.new(_AI_GENERATED_COLLECTION)

    if collection.name not in {child.name for child in scene.collection.children}:
        scene.collection.children.link(collection)

    return collection


def _delete_ai_generated_objects(scene):
    removed_count = 0
    for obj in list(scene.objects):
        if not obj.get(_AI_GENERATED_MARKER):
            continue
        bpy.data.objects.remove(obj, do_unlink=True)
        removed_count += 1

    return removed_count


def _stage_bounds_box(scene):
    stage_bounds = next((obj for obj in scene.objects if obj.name.startswith("StageBounds")), None)
    if stage_bounds:
        center = stage_bounds.matrix_world.translation.copy()
        dimensions = stage_bounds.dimensions
        extents = Vector((
            max(1.0, abs(dimensions.x) * 0.5),
            max(1.0, abs(dimensions.y) * 0.5),
            max(1.0, abs(dimensions.z) * 0.5),
        ))
        return center, extents

    return Vector((0.0, 0.0, 0.0)), Vector((18.0, 22.0, 10.0))


def _find_player_spawn_position(scene, fallback_center, fallback_extents):
    for obj in scene.objects:
        if getattr(obj, "game_obj_type", "NONE") == "PLAYER":
            return obj.matrix_world.translation.copy()

    return fallback_center + Vector((0.0, -fallback_extents.y * 0.55, 0.0))


def _safe_normalized(vector, fallback):
    if vector.length <= 0.0001:
        return fallback.copy()
    return vector.normalized()


def _clamp_point_to_box(point, center, extents, margin):
    def clamp_axis(value, center_value, extent_value, margin_value):
        usable_extent = max(0.0, extent_value - margin_value)
        return max(center_value - usable_extent, min(center_value + usable_extent, value))

    return Vector((
        clamp_axis(point.x, center.x, extents.x, margin.x),
        clamp_axis(point.y, center.y, extents.y, margin.y),
        clamp_axis(point.z, center.z, extents.z, margin.z),
    ))


def _prompt_has(prompt, *keywords):
    return any(keyword.casefold() in prompt for keyword in keywords)


def _parse_ai_motion_prompt(prompt, style):
    normalized = str(prompt or "").casefold()
    motion = {
        "pattern": "DEFAULT",
        "vertical": "NONE",
        "turn_sign": 1.0,
        "loop": None,
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

    if wants_edge and wants_orbit:
        motion["pattern"] = "EDGE_ORBIT"
        motion["loop"] = True
    elif _prompt_has(normalized, "螺旋", "スパイラル", "spiral"):
        motion["pattern"] = "SPIRAL"
        motion["loop"] = True
        motion["amplitude_multiplier"] = 1.15
    elif _prompt_has(normalized, "ジグザグ", "蛇行", "左右", "s字", "s-字", "zigzag", "zig-zag"):
        motion["pattern"] = "ZIGZAG"
        motion["amplitude_multiplier"] = 1.25
    elif wants_orbit:
        motion["pattern"] = "ORBIT"
        motion["loop"] = True
    elif _prompt_has(normalized, "突撃", "直線", "まっすぐ", "正面", "接近", "rush", "charge", "straight"):
        motion["pattern"] = "STRAIGHT"
        motion["loop"] = False

    if _prompt_has(normalized, "上昇", "上に", "上が", "登", "高く", "climb", "up"):
        motion["vertical"] = "UP"
    elif _prompt_has(normalized, "下降", "下に", "下が", "降下", "低く", "dive", "down"):
        motion["vertical"] = "DOWN"
    elif _prompt_has(normalized, "上下", "波", "うね", "wave", "sine"):
        motion["vertical"] = "WAVE"

    if _prompt_has(normalized, "高速", "速く", "早く", "fast", "quick"):
        motion["speed_multiplier"] *= 1.25
    if _prompt_has(normalized, "低速", "ゆっくり", "slow"):
        motion["speed_multiplier"] *= 0.75

    if _prompt_has(normalized, "大きく", "広く", "ワイド", "wide"):
        motion["amplitude_multiplier"] *= 1.25
    if _prompt_has(normalized, "小さく", "狭く", "tight", "small"):
        motion["amplitude_multiplier"] *= 0.75

    if _prompt_has(normalized, "同時", "一斉", "まとめて", "simultaneous"):
        motion["opener"] = "ALL"
    elif _prompt_has(normalized, "順番", "一体ずつ", "wave", "sequential"):
        motion["opener"] = "ONE"

    if motion["pattern"] == "DEFAULT" and style == 'PATROL':
        motion["pattern"] = "ORBIT"
        motion["loop"] = True
    return motion


def _apply_vertical_motion(points, motion, extents):
    vertical = motion.get("vertical", "NONE")
    if vertical == "NONE" or len(points) <= 1:
        return points

    max_offset = min(extents.z * 0.28, 4.0)
    adjusted = []
    for index, point in enumerate(points):
        t = index / (len(points) - 1)
        z_offset = 0.0
        if vertical == "UP":
            z_offset = max_offset * t
        elif vertical == "DOWN":
            z_offset = -max_offset * t
        elif vertical == "WAVE":
            z_offset = max_offset * (1.0 if index % 2 == 1 else -0.55)
        adjusted.append(point + Vector((0.0, 0.0, z_offset)))
    return adjusted


def _build_ai_formation_offsets(count, extents, motion):
    if not motion.get("keep_formation"):
        return [Vector((0.0, 0.0, 0.0)) for _ in range(count)]

    columns = max(1, math.ceil(math.sqrt(count)))
    rows = max(1, math.ceil(count / columns))
    max_spacing_x = (extents.x * 0.60) / max(1, columns - 1)
    max_spacing_y = (extents.y * 0.60) / max(1, rows - 1)
    spacing = min(2.0, max_spacing_x, max_spacing_y)
    spacing = max(0.65, spacing)

    offsets = []
    for index in range(count):
        col = index % columns
        row = index // columns
        offsets.append(Vector((
            (col - (columns - 1) * 0.5) * spacing,
            (row - (rows - 1) * 0.5) * spacing,
            0.0,
        )))

    motion["formation_extent_x"] = max((abs(offset.x) for offset in offsets), default=0.0)
    motion["formation_extent_y"] = max((abs(offset.y) for offset in offsets), default=0.0)
    return offsets


def _build_edge_orbit_points(motion, center, extents, formation_offset):
    edge_margin = float(motion.get("edge_margin", 2.0))
    formation_extent_x = float(motion.get("formation_extent_x", 0.0))
    formation_extent_y = float(motion.get("formation_extent_y", 0.0))
    radius_x = max(1.0, extents.x - edge_margin - formation_extent_x)
    radius_y = max(1.0, extents.y - edge_margin - formation_extent_y)

    if float(motion.get("turn_sign", 1.0)) < 0.0:
        corners = [
            (radius_x, -radius_y),
            (0.0, -radius_y),
            (-radius_x, -radius_y),
            (-radius_x, 0.0),
            (-radius_x, radius_y),
            (0.0, radius_y),
            (radius_x, radius_y),
            (radius_x, 0.0),
        ]
    else:
        corners = [
            (-radius_x, -radius_y),
            (0.0, -radius_y),
            (radius_x, -radius_y),
            (radius_x, 0.0),
            (radius_x, radius_y),
            (0.0, radius_y),
            (-radius_x, radius_y),
            (-radius_x, 0.0),
        ]

    return [
        center + Vector((x, y, 0.0)) + formation_offset
        for x, y in corners
    ]


def _create_ai_enemy_path(collection, path_id, points, loop, speed, handle_type='AUTO'):
    curve = bpy.data.curves.new(path_id, 'CURVE')
    curve.dimensions = '3D'
    curve.resolution_u = 24
    curve.bevel_depth = 0.04
    curve.bevel_resolution = 2

    spline = curve.splines.new('BEZIER')
    spline.bezier_points.add(len(points) - 1)
    spline.use_cyclic_u = loop

    for point, co in zip(spline.bezier_points, points):
        point.co = co
        point.handle_left_type = handle_type
        point.handle_right_type = handle_type

    obj = bpy.data.objects.new(path_id, curve)
    collection.objects.link(obj)
    obj.enemy_path_id = path_id
    obj.enemy_path_loop = loop
    obj.enemy_path_speed = speed
    obj[_AI_GENERATED_MARKER] = True
    return obj


def _create_ai_enemy_spawn(collection, name, location, facing, enemy_type, path_id, trigger_name, delay_frames):
    obj = bpy.data.objects.new(name, None)
    collection.objects.link(obj)
    obj.empty_display_type = 'SINGLE_ARROW'
    obj.empty_display_size = 2.0
    obj.location = location

    facing_xy = Vector((facing.x, facing.y, 0.0))
    if facing_xy.length > 0.0001:
        obj.rotation_euler = (0.0, 0.0, math.atan2(facing_xy.y, facing_xy.x) - math.pi * 0.5)

    obj.game_obj_type = 'ENEMY'
    obj.enemy_type = enemy_type
    obj.enemy_path_id = path_id
    obj.enemy_reinforcement_trigger_name = trigger_name
    obj.enemy_reinforcement_delay_frames = delay_frames
    obj[_AI_GENERATED_MARKER] = True
    return obj


def _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, pair_index, rng, formation_offset=None):
    if formation_offset is None:
        formation_offset = Vector((0.0, 0.0, 0.0))

    margin = Vector((1.5, 1.5, 1.0))
    to_player = player - spawn
    flat_to_player = Vector((to_player.x, to_player.y, 0.0))
    forward = _safe_normalized(flat_to_player, Vector((0.0, -1.0, 0.0)))
    right = Vector((forward.y, -forward.x, 0.0))
    amplitude = float(motion.get("amplitude_multiplier", 1.0))
    turn_side = side * float(motion.get("turn_sign", 1.0))
    lateral = min(extents.x * 0.45, 8.0) * side * amplitude
    z_swing = min(extents.z * 0.18, 2.5)
    pattern = motion.get("pattern", "DEFAULT")

    if pattern == "EDGE_ORBIT":
        raw_points = _build_edge_orbit_points(motion, center, extents, formation_offset)
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        edge_margin = float(motion.get("edge_margin", 2.0))
        return [_clamp_point_to_box(point, center, extents, Vector((edge_margin, edge_margin, 1.0))) for point in raw_points]

    if pattern == "SPIRAL":
        focus = player + forward * min(extents.y * 0.12, 3.0)
        radius_x = min(extents.x * (0.22 + pair_index * 0.025), 7.5) * amplitude
        radius_y = min(extents.y * (0.16 + pair_index * 0.025), 7.5) * amplitude
        raw_points = [
            spawn,
            focus + right * (radius_x * turn_side) + Vector((0.0, 0.0, -z_swing)),
            focus + forward * radius_y - right * (radius_x * turn_side * 0.45),
            focus - right * (radius_x * turn_side) + Vector((0.0, 0.0, z_swing)),
            focus - forward * radius_y + right * (radius_x * turn_side * 0.45),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if pattern == "ORBIT":
        focus = player + forward * min(extents.y * 0.08, 2.5)
        radius_x = min(extents.x * (0.24 + pair_index * 0.03), 8.0) * amplitude
        radius_y = min(extents.y * (0.18 + pair_index * 0.025), 8.0) * amplitude
        raw_points = [
            spawn,
            focus + right * (radius_x * turn_side),
            focus + forward * radius_y + Vector((0.0, 0.0, z_swing)),
            focus - right * (radius_x * turn_side),
            focus - forward * radius_y + Vector((0.0, 0.0, -z_swing)),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if pattern == "ZIGZAG":
        step = min(max((player - spawn).length / 4.0, 3.0), extents.y * 0.28)
        zigzag_width = min(extents.x * 0.28, 6.0) * amplitude * side
        raw_points = [spawn]
        for point_index in range(1, 5):
            direction = -1.0 if point_index % 2 == 0 else 1.0
            raw_points.append(
                spawn
                + forward * (step * point_index)
                + right * (zigzag_width * direction)
                + Vector((0.0, 0.0, z_swing * direction * 0.7))
            )
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if pattern == "STRAIGHT":
        overshoot = min(extents.y * 0.24, 6.0)
        raw_points = [
            spawn,
            spawn + forward * min(extents.y * 0.24, 6.0) + Vector((0.0, 0.0, z_swing * 0.25)),
            player + forward * overshoot,
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if style == 'PATROL':
        radius_x = min(extents.x * 0.25 + pair_index * 0.4, 7.0)
        radius_y = min(extents.y * 0.18 + pair_index * 0.3, 8.0)
        raw_points = [
            spawn,
            spawn + forward * radius_y + right * (radius_x * side),
            spawn + forward * (radius_y * 0.2) - right * (radius_x * side),
            spawn - forward * radius_y,
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if style == 'SWARM':
        lateral *= 0.45
        raw_points = [
            spawn,
            player + forward * min(extents.y * 0.22, 5.0) + right * lateral + Vector((0.0, 0.0, z_swing)),
            player - right * lateral + Vector((0.0, 0.0, -z_swing)),
            spawn - forward * min(extents.y * 0.22, 5.0),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if style == 'AMBUSH':
        lateral *= 1.15
        raw_points = [
            spawn,
            center + right * lateral + Vector((0.0, 0.0, z_swing)),
            player - right * (lateral * 0.65) + Vector((0.0, 0.0, -z_swing)),
            center - forward * min(extents.y * 0.38, 9.0) - right * (lateral * 0.35),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    raw_points = [
        spawn,
        center + right * lateral + Vector((0.0, 0.0, z_swing)),
        player + forward * min(extents.y * 0.2, 6.0) - right * (lateral * 0.45),
        center - forward * min(extents.y * 0.35, 8.0) - right * (lateral * 0.25),
    ]
    raw_points = _apply_vertical_motion(raw_points, motion, extents)
    return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]


def _gemini_enemy_plan_schema():
    point_schema = {
        "type": "object",
        "properties": {
            "x": {"type": "number"},
            "y": {"type": "number"},
            "z": {"type": "number"},
        },
        "required": ["x", "y", "z"],
    }
    return {
        "type": "object",
        "properties": {
            "enemies": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "spawn": point_schema,
                        "path": {
                            "type": "array",
                            "items": point_schema,
                        },
                        "loop": {"type": "boolean"},
                        "speed": {"type": "number"},
                        "enemy_type": {
                            "type": "string",
                            "enum": ["VF1", "VF1-1"],
                        },
                        "trigger_index": {"type": "integer"},
                        "delay_frames": {"type": "integer"},
                        "handle_type": {
                            "type": "string",
                            "enum": ["AUTO", "VECTOR"],
                        },
                    },
                    "required": ["spawn", "path", "loop", "speed", "enemy_type"],
                },
            },
            "notes": {"type": "string"},
        },
        "required": ["enemies"],
    }


def _resolve_gemini_api_key(scene):
    configured_key = getattr(scene, "myaddon_ai_enemy_gemini_api_key", "").strip()
    if configured_key:
        return configured_key
    return os.environ.get("GEMINI_API_KEY", "").strip()


def _gemini_model_name(scene):
    model = str(getattr(scene, "myaddon_ai_enemy_gemini_model", "gemini-3.5-flash")).strip()
    if not model:
        model = "gemini-3.5-flash"
    if model.startswith("models/"):
        model = model[len("models/"):]
    return model


def _stage_bounds_prompt(center, extents):
    return {
        "center": [center.x, center.y, center.z],
        "extents": [extents.x, extents.y, extents.z],
        "min": [center.x - extents.x, center.y - extents.y, center.z - extents.z],
        "max": [center.x + extents.x, center.y + extents.y, center.z + extents.z],
    }


def _build_gemini_enemy_prompt(count, seed, style, motion_prompt, wave_delay, center, extents, player):
    style_label = {
        'BALANCED': "バランス",
        'AMBUSH': "挟み撃ち",
        'SWARM': "群れ",
        'PATROL': "巡回",
    }.get(style, style)
    bounds = _stage_bounds_prompt(center, extents)
    player_data = {"x": player.x, "y": player.y, "z": player.z}
    return (
        "You are a level-design route generator for a 3D shooting game.\n"
        "Return only JSON that matches the provided schema. Do not wrap it in markdown.\n"
        "Coordinate system: X is left/right, Y is field depth, Z is height. Unit is Blender world unit.\n"
        f"Enemy count must be exactly {count}.\n"
        f"Seed hint: {seed}. Style: {style_label}. Default reinforcement delay: {wave_delay} frames.\n"
        f"Field bounds JSON: {json.dumps(bounds, ensure_ascii=False)}\n"
        f"Player spawn JSON: {json.dumps(player_data, ensure_ascii=False)}\n"
        f"Designer request: {motion_prompt}\n"
        "Rules:\n"
        "- Every spawn and path point must stay inside the field bounds.\n"
        "- spawn should be the first path point or very close to it.\n"
        "- Each path must contain 3 to 10 points.\n"
        "- Use loop=true for patrol, orbit, lap, circle, one-lap, or field-edge movement.\n"
        "- Use handle_type=VECTOR for paths that must not bulge outside the field.\n"
        "- Use speed from 0.015 to 0.18. If the request says very fast or explosive speed, use a higher value.\n"
        "- trigger_index is -1 for enemies that appear immediately. Otherwise use a previous enemy index.\n"
        "- If the request says to keep a swarm, formation, or group together, keep route shapes similar and offset the points slightly.\n"
        "- If the request says field edge or one lap, create a route around the inside edge without leaving the field.\n"
    )


def _parse_gemini_json_text(text):
    stripped = str(text or "").strip()
    if stripped.startswith("```"):
        lines = stripped.splitlines()
        if lines and lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].startswith("```"):
            lines = lines[:-1]
        stripped = "\n".join(lines).strip()

    try:
        return json.loads(stripped)
    except json.JSONDecodeError:
        start = stripped.find("{")
        end = stripped.rfind("}")
        if start >= 0 and end > start:
            return json.loads(stripped[start:end + 1])
        raise


def _request_gemini_enemy_plan(scene, count, seed, style, motion_prompt, wave_delay, center, extents, player):
    api_key = _resolve_gemini_api_key(scene)
    if not api_key:
        raise ValueError("Gemini APIキーが未設定です。")

    model = urllib.parse.quote(_gemini_model_name(scene), safe="/")
    url = f"https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent"
    prompt = _build_gemini_enemy_prompt(count, seed, style, motion_prompt, wave_delay, center, extents, player)
    payload = {
        "contents": [
            {
                "parts": [
                    {"text": prompt},
                ],
            },
        ],
        "generationConfig": {
            "responseMimeType": "application/json",
            "responseSchema": _gemini_enemy_plan_schema(),
            "maxOutputTokens": 4096,
        },
    }
    request = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
            "x-goog-api-key": api_key,
        },
        method="POST",
    )

    timeout = max(5, int(getattr(scene, "myaddon_ai_enemy_gemini_timeout", 25)))
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            response_data = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"Gemini APIエラー {exc.code}: {detail[:280]}") from exc
    except urllib.error.URLError as exc:
        raise RuntimeError(f"Gemini APIへ接続できません: {exc.reason}") from exc

    candidates = response_data.get("candidates", [])
    if not candidates:
        raise RuntimeError("Geminiから候補が返ってきませんでした。")

    parts = candidates[0].get("content", {}).get("parts", [])
    text = "".join(part.get("text", "") for part in parts)
    if not text.strip():
        raise RuntimeError("Geminiの返答が空でした。")
    return _parse_gemini_json_text(text)


def _coerce_ai_point(value):
    if isinstance(value, dict):
        raw = (value.get("x"), value.get("y"), value.get("z"))
    elif isinstance(value, (list, tuple)) and len(value) >= 3:
        raw = (value[0], value[1], value[2])
    else:
        return None

    try:
        coords = tuple(float(component) for component in raw)
    except (TypeError, ValueError):
        return None
    if not all(math.isfinite(component) for component in coords):
        return None
    return Vector(coords)


def _clamp_float(value, default, minimum, maximum):
    try:
        number = float(value)
    except (TypeError, ValueError):
        number = default
    if not math.isfinite(number):
        number = default
    return max(minimum, min(maximum, number))


def _fallback_second_path_point(spawn, player, center, extents):
    forward = _safe_normalized(player - spawn, Vector((0.0, 1.0, 0.0)))
    distance = min(max(extents.y * 0.20, 2.0), 6.0)
    point = _clamp_point_to_box(spawn + forward * distance, center, extents, Vector((1.0, 1.0, 0.8)))
    if (point - spawn).length < 0.05:
        point = _clamp_point_to_box(spawn + Vector((distance, 0.0, 0.0)), center, extents, Vector((1.0, 1.0, 0.8)))
    return point


def _sanitize_gemini_enemy_plan(plan_data, count, center, extents, player):
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
            points.append(_fallback_second_path_point(spawn, player, center, extents))

        enemy_type = str(enemy.get("enemy_type", "")).strip()
        if enemy_type not in {"VF1", "VF1-1"}:
            enemy_type = "VF1-1" if index % 3 == 2 else "VF1"

        try:
            trigger_index = int(enemy.get("trigger_index", -1))
        except (TypeError, ValueError):
            trigger_index = -1
        if trigger_index < 0 or trigger_index >= index:
            trigger_index = -1

        try:
            delay_frames = int(enemy.get("delay_frames", 0))
        except (TypeError, ValueError):
            delay_frames = 0
        delay_frames = max(0, min(3600, delay_frames))

        handle_type = str(enemy.get("handle_type", "AUTO")).upper()
        if handle_type not in {"AUTO", "VECTOR"}:
            handle_type = "AUTO"

        blueprints.append({
            "spawn": spawn,
            "points": points,
            "loop": bool(enemy.get("loop", False)),
            "speed": _clamp_float(enemy.get("speed", 0.05), 0.05, 0.001, 0.20),
            "enemy_type": enemy_type,
            "trigger_index": trigger_index,
            "delay_frames": delay_frames,
            "handle_type": handle_type,
        })

    if not blueprints:
        raise ValueError("Geminiの敵データを有効な座標に変換できませんでした。")
    if len(blueprints) < count:
        raise ValueError(f"Geminiの有効な敵データが不足しています: {len(blueprints)}/{count}")
    return blueprints


def _path_start_facing(points, fallback):
    start = points[0] if points else Vector((0.0, 0.0, 0.0))
    for point in points[1:]:
        direction = point - start
        if direction.length > 0.05:
            return direction
    return fallback


def _create_ai_enemy_objects_from_blueprints(scene, collection, blueprints, motion_prompt, player, provider_label):
    generated_objects = []
    generated_enemies = []
    for index, blueprint in enumerate(blueprints):
        spawn = blueprint["spawn"]
        points = blueprint["points"]
        path_id = _unique_enemy_path_id(scene)
        path_obj = _create_ai_enemy_path(
            collection,
            path_id,
            points,
            blueprint["loop"],
            blueprint["speed"],
            blueprint["handle_type"],
        )
        path_obj["myaddon_ai_motion_prompt"] = motion_prompt
        path_obj["myaddon_ai_provider"] = provider_label

        trigger_name = ""
        trigger_index = blueprint.get("trigger_index", -1)
        if 0 <= trigger_index < len(generated_enemies):
            trigger_name = generated_enemies[trigger_index].name

        enemy_name = f"AIEnemy_{index + 1:02d}"
        enemy_obj = _create_ai_enemy_spawn(
            collection,
            enemy_name,
            spawn,
            _path_start_facing(points, player - spawn),
            blueprint["enemy_type"],
            path_id,
            trigger_name,
            blueprint.get("delay_frames", 0),
        )
        enemy_obj["myaddon_ai_motion_prompt"] = motion_prompt
        enemy_obj["myaddon_ai_provider"] = provider_label

        generated_objects.extend([path_obj, enemy_obj])
        generated_enemies.append(enemy_obj)

    return generated_objects, generated_enemies


def _is_unset_text(value):
    return value is None or str(value).strip() in {"", "None"}


def _build_object_data(obj, model_filenames=None):
    trans, rot, scale = _get_world_transform(obj)
    rot = rot.to_euler()
    rot.x, rot.y, rot.z = [math.degrees(v) for v in rot]

    export_scale = scale
    if obj.name.startswith("StageBounds"):
        dimensions = obj.dimensions
        export_scale = (
            dimensions.x * 0.5,
            dimensions.y * 0.5,
            dimensions.z * 0.5,
        )

    obj_data = {
        "type": obj.type,
        "name": obj.name,
        "transform": {
            "translation": [trans.x, trans.y, trans.z],
            "rotation": [rot.x, rot.y, rot.z],
            "scale": [export_scale[0], export_scale[1], export_scale[2]],
        },
    }

    if hasattr(obj, "collider") and obj.collider != "None":
        if obj.collider in ["Box", "BOX"]:
            center = obj.collider_center
            size = obj.collider_size
            obj_data["collider"] = {
                "type": obj.collider,
                "center": [center[0], center[1], center[2]],
                "size": [size[0], size[1], size[2]],
            }

    if hasattr(obj, "game_obj_type") and obj.game_obj_type != 'NONE':
        obj_data["category"] = obj.game_obj_type

    if obj.type == 'MESH' and model_filenames and obj.name in model_filenames:
        obj_data["model"] = model_filenames[obj.name]

    if hasattr(obj, "enemy_type") and obj.enemy_type != "None":
        obj_data["enemy"] = {
            "type": obj.enemy_type,
        }

    if getattr(obj, "game_obj_type", "NONE") == 'ENEMY':
        path_id = getattr(obj, "enemy_path_id", "None")
        if path_id != "None":
            obj_data["path_id"] = path_id

        trigger_name = getattr(obj, "enemy_reinforcement_trigger_name", "")
        if not _is_unset_text(trigger_name):
            obj_data["reinforcement"] = {
                "trigger": str(trigger_name).strip(),
                "delay": max(0, int(getattr(obj, "enemy_reinforcement_delay_frames", 0))),
            }

    if obj.type == 'MESH':
        mesh = obj.to_mesh()
        obj_data["vertices_count"] = len(mesh.vertices)
        obj.to_mesh_clear()
    else:
        obj_data["vertices_count"] = 0

    return obj_data


def _build_path_data(obj):
    return {
        "id": _get_enemy_path_id(obj),
        "name": obj.name,
        "loop": bool(getattr(obj, "enemy_path_loop", False)),
        "speed": float(getattr(obj, "enemy_path_speed", 0.05)),
        "points": _get_curve_world_points(obj),
    }


def _build_scene_data(scene, model_filenames=None):
    if model_filenames is None:
        model_filenames = _build_model_filename_map(scene)

    errors, warnings = validation.validate_scene(scene)

    scene_data = {
        "name": "scene",
        "objects": [],
        "paths": [],
        "validation": {
            "status": _make_validation_status(errors, warnings),
            "errors": errors,
            "warnings": warnings,
            "checks": validation.build_check_report_text(errors, warnings),
        },
    }

    for obj in scene.objects:
        if obj.type in ['MESH', 'EMPTY']:
            scene_data["objects"].append(_build_object_data(obj, model_filenames))
        elif _is_enemy_path_object(obj):
            scene_data["paths"].append(_build_path_data(obj))

    return scene_data


def _make_validation_status(errors, warnings):
    if errors:
        return f"エラー {len(errors)}件 / 警告 {len(warnings)}件"
    if warnings:
        return f"警告 {len(warnings)}件"
    return "OK"


def _export_scene_to_path(scene, filepath, model_filenames=None):
    with open(filepath, "wt", encoding="utf-8") as file:
        json.dump(_build_scene_data(scene, model_filenames), file, ensure_ascii=False, indent=4)


def _export_obj_with_current_selection(filepath):
    try:
        return bpy.ops.wm.obj_export(
            filepath=filepath,
            export_selected_objects=True,
            export_materials=True,
            export_uv=True,
            export_normals=True,
            export_triangulated_mesh=True,
            apply_modifiers=True,
            path_mode='AUTO',
            forward_axis='NEGATIVE_Z',
            up_axis='Y',
        )
    except (AttributeError, TypeError):
        try:
            return bpy.ops.export_scene.obj(
                filepath=filepath,
                use_selection=True,
                use_materials=True,
                use_mesh_modifiers=True,
                use_triangles=True,
                use_uvs=True,
                use_normals=True,
                path_mode='AUTO',
                axis_forward='-Z',
                axis_up='Y',
            )
        except AttributeError:
            bpy.ops.preferences.addon_enable(module="io_scene_obj")
            return bpy.ops.export_scene.obj(
                filepath=filepath,
                use_selection=True,
                use_materials=True,
                use_mesh_modifiers=True,
                use_triangles=True,
                use_uvs=True,
                use_normals=True,
                path_mode='AUTO',
                axis_forward='-Z',
                axis_up='Y',
            )


def _export_visible_meshes_to_obj(scene, filepath):
    export_objects = [obj for obj in scene.objects if _is_obj_export_target(obj)]
    if not export_objects:
        return 0

    context = bpy.context
    view_layer = context.view_layer
    previous_active = view_layer.objects.active
    previous_selection = list(context.selected_objects)
    previous_mode = context.object.mode if context.object else None

    try:
        if context.object and context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        bpy.ops.object.select_all(action='DESELECT')
        for obj in export_objects:
            obj.select_set(True)
        view_layer.objects.active = export_objects[0]

        _export_obj_with_current_selection(filepath)
        return len(export_objects)
    finally:
        if context.object and context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        bpy.ops.object.select_all(action='DESELECT')
        for obj in previous_selection:
            if obj.name in bpy.data.objects:
                obj.select_set(True)

        if previous_active and previous_active.name in bpy.data.objects:
            view_layer.objects.active = previous_active

        if previous_mode and previous_mode != 'OBJECT' and view_layer.objects.active:
            try:
                bpy.ops.object.mode_set(mode=previous_mode)
            except Exception:
                pass


def _export_single_mesh_to_obj(obj, filepath):
    context = bpy.context
    view_layer = context.view_layer
    previous_active = view_layer.objects.active
    previous_selection = list(context.selected_objects)
    previous_mode = context.object.mode if context.object else None
    temp_obj = None
    temp_mesh = None

    try:
        if context.object and context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        depsgraph = context.evaluated_depsgraph_get()
        evaluated_obj = obj.evaluated_get(depsgraph)
        temp_mesh = bpy.data.meshes.new_from_object(evaluated_obj, depsgraph=depsgraph)
        temp_obj = bpy.data.objects.new(obj.name, temp_mesh)
        temp_obj.matrix_world.identity()
        context.collection.objects.link(temp_obj)

        bpy.ops.object.select_all(action='DESELECT')
        temp_obj.select_set(True)
        view_layer.objects.active = temp_obj

        _export_obj_with_current_selection(filepath)
    finally:
        if context.object and context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        if temp_obj and temp_obj.name in bpy.data.objects:
            bpy.data.objects.remove(temp_obj, do_unlink=True)
        if temp_mesh and temp_mesh.name in bpy.data.meshes and temp_mesh.users == 0:
            bpy.data.meshes.remove(temp_mesh)

        bpy.ops.object.select_all(action='DESELECT')
        for selected_obj in previous_selection:
            if selected_obj.name in bpy.data.objects:
                selected_obj.select_set(True)

        if previous_active and previous_active.name in bpy.data.objects:
            view_layer.objects.active = previous_active

        if previous_mode and previous_mode != 'OBJECT' and view_layer.objects.active:
            try:
                bpy.ops.object.mode_set(mode=previous_mode)
            except Exception:
                pass


def _export_individual_meshes_to_obj(scene, model_filenames):
    if not bpy.data.filepath:
        return 0

    blend_dir = os.path.dirname(bpy.data.filepath)
    exported_count = 0

    for obj in scene.objects:
        filename = model_filenames.get(obj.name)
        if not filename:
            continue

        _export_single_mesh_to_obj(obj, os.path.join(blend_dir, filename))
        exported_count += 1

    return exported_count


def _auto_export_scene_json(model_filenames=None):
    filepath = _default_scene_json_path()
    errors, warnings = validation.validate_scene(bpy.context.scene)
    if errors:
        print(f"scene.json 自動出力をスキップしました。バリデーションエラー {len(errors)}件があります。")
        return

    try:
        _export_scene_to_path(bpy.context.scene, filepath, model_filenames)
        if warnings:
            print(f"scene.json を自動出力しました（警告 {len(warnings)}件）: {filepath}")
        else:
            print(f"scene.json を自動出力しました: {filepath}")
    except Exception as exc:
        print(f"scene.json 自動出力に失敗しました: {exc}")


def _auto_export_obj(model_filenames=None):
    scene = bpy.context.scene
    if model_filenames is None:
        model_filenames = _build_model_filename_map(scene)

    try:
        exported_count = _export_individual_meshes_to_obj(scene, model_filenames)
        if exported_count == 0:
            print("OBJ/MTL 自動出力をスキップしました。表示中のメッシュがありません。")
        else:
            print(f"個別OBJ/MTLを自動出力しました（{exported_count}個のメッシュ）")
    except Exception as exc:
        print(f"OBJ/MTL 自動出力に失敗しました: {exc}")


def _unregister_auto_export_handler():
    for handler in list(bpy.app.handlers.save_post):
        if getattr(handler, "__name__", "") in {
            "_auto_export_scene_json_on_save",
            "_auto_export_assets_on_save",
        }:
            bpy.app.handlers.save_post.remove(handler)


@persistent
def _auto_export_assets_on_save(_dummy):
    if not bpy.data.filepath:
        return

    model_filenames = _build_model_filename_map(bpy.context.scene)
    _auto_export_scene_json(model_filenames)
    _auto_export_obj(model_filenames)


class MYADDON_OT_stretch_vertex(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_stretch_vertex"
    bl_label = "頂点を伸ばす"
    bl_description = "頂点座標を引っ張って伸ばします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.data.objects["Cube"].data.vertices[0].co.x += 1.0
        print("頂点を伸ばしました。")
        return {'FINISHED'}


class MYADDON_OT_create_ico_sphere(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_object"
    bl_label = "ICO球生成"
    bl_description = "ICO球を生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_ico_sphere_add()
        print("ICO球を生成しました。")
        return {'FINISHED'}


class MYADDON_OT_export_scene(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    bl_idname = "myaddon.myaddon_ot_export_scene"
    bl_label = "シーン出力"
    bl_description = "シーン情報をゲーム用scene.jsonへ出力します"

    filename_ext = ".json"

    def _build_object_data(self, obj):
        return _build_object_data(obj)

    def _build_path_data(self, obj):
        return _build_path_data(obj)

    def export(self):
        if not self.filepath:
            self.filepath = _default_scene_json_path()

        _export_scene_to_path(bpy.context.scene, self.filepath)

    def invoke(self, context, event):
        self.filepath = _default_scene_json_path()
        return self.execute(context)

    def execute(self, context):
        if not self.filepath:
            self.filepath = _default_scene_json_path()

        errors, warnings = validation.validate_and_store(context.scene)
        if errors:
            self.report({'ERROR'}, "バリデーションエラーがあります。詳細はパネルを確認してください。")
            return {'CANCELLED'}

        self.export()
        if warnings:
            self.report({'WARNING'}, f"警告 {len(warnings)}件がありますが、シーン情報をExportしました: {self.filepath}")
            return {'FINISHED'}

        self.report({'INFO'}, f"シーン情報をExportしました: {self.filepath}")
        return {'FINISHED'}


class MYADDON_OT_validate_scene(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_validate_scene"
    bl_label = "シーンチェック"
    bl_description = "現在のシーンをゲーム用データとして使えるかチェックします"
    bl_options = {'REGISTER'}

    def execute(self, context):
        errors, warnings = validation.validate_and_store(context.scene)
        if errors:
            self.report({'ERROR'}, f"バリデーションエラー {len(errors)}件")
        elif warnings:
            self.report({'WARNING'}, f"バリデーション警告 {len(warnings)}件")
        else:
            self.report({'INFO'}, "バリデーションOK")
        return {'FINISHED'}


class MYADDON_OT_playtest_game(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_playtest_game"
    bl_label = "ゲームをプレイ"
    bl_description = "現在の配置を出力してゲームを起動します"
    bl_options = {'REGISTER'}

    def execute(self, context):
        scene = context.scene
        errors, warnings = validation.validate_and_store(scene)
        if errors:
            self.report({'ERROR'}, "バリデーションエラーがあります。修正してからPlaytestしてください。")
            return {'CANCELLED'}

        model_filenames = _build_model_filename_map(scene)
        _export_scene_to_path(scene, _default_scene_json_path(), model_filenames)
        if bpy.data.filepath:
            _export_individual_meshes_to_obj(scene, model_filenames)

        exe_path = _resolve_game_exe_path(scene)
        if not os.path.isfile(exe_path):
            self.report({'ERROR'}, f"ゲームEXEが見つかりません: {exe_path}")
            return {'CANCELLED'}

        working_dir = _default_game_working_dir()
        if not os.path.isdir(working_dir):
            working_dir = os.path.dirname(exe_path)

        try:
            subprocess.Popen([exe_path], cwd=working_dir)
        except Exception as exc:
            self.report({'ERROR'}, f"ゲーム起動に失敗しました: {exc}")
            return {'CANCELLED'}

        if warnings:
            self.report({'WARNING'}, f"警告 {len(warnings)}件がありますが、ゲームを起動しました。")
        else:
            self.report({'INFO'}, "ゲームを起動しました。")
        return {'FINISHED'}


class MYADDON_OT_apply_active_properties_to_selection(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_apply_active_properties_to_selection"
    bl_label = "選択中へ一括適用"
    bl_description = "アクティブオブジェクトのゲーム用設定を、選択中の他オブジェクトへコピーします"
    bl_options = {'REGISTER', 'UNDO'}

    copy_collider: bpy.props.BoolProperty(name="コライダーもコピー", default=True)

    def execute(self, context):
        active = context.active_object
        if not active:
            self.report({'ERROR'}, "コピー元にするオブジェクトをアクティブにしてください。")
            return {'CANCELLED'}

        targets = [obj for obj in context.selected_objects if obj != active]
        if not targets:
            self.report({'ERROR'}, "コピー先にするオブジェクトも一緒に選択してください。")
            return {'CANCELLED'}

        property_names = [
            "game_obj_type",
            "enemy_type",
            "enemy_path_id",
        ]

        if self.copy_collider:
            property_names.extend([
                "collider",
                "collider_center",
                "collider_size",
            ])

        if _is_enemy_path_object(active):
            property_names.extend([
                "enemy_path_loop",
                "enemy_path_speed",
            ])

        copied_count = 0
        for target in targets:
            for property_name in property_names:
                if not hasattr(active, property_name) or not hasattr(target, property_name):
                    continue
                value = getattr(active, property_name)
                if hasattr(value, "__len__") and not isinstance(value, str):
                    value = tuple(value)
                setattr(target, property_name, value)
            copied_count += 1

        validation.validate_and_store(context.scene)
        self.report({'INFO'}, f"{copied_count}個のオブジェクトへ設定を一括適用しました。")
        return {'FINISHED'}


class MYADDON_OT_ai_generate_enemy_plan(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_generate_enemy_plan"
    bl_label = "AI敵プラン生成"
    bl_description = "敵の位置、飛行ルート、登場パターンを自動生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        scene = context.scene

        collection = _get_ai_collection(scene)
        center, extents = _stage_bounds_box(scene)
        player = _find_player_spawn_position(scene, center, extents)
        player_forward = _safe_normalized(
            Vector((center.x - player.x, center.y - player.y, 0.0)),
            Vector((0.0, 1.0, 0.0)),
        )
        player_right = Vector((player_forward.y, -player_forward.x, 0.0))

        count = max(1, int(getattr(scene, "myaddon_ai_enemy_count", 6)))
        seed = max(0, int(getattr(scene, "myaddon_ai_enemy_seed", 1)))
        style = getattr(scene, "myaddon_ai_enemy_style", 'BALANCED')
        motion_prompt = getattr(scene, "myaddon_ai_enemy_motion_prompt", "")
        motion = _parse_ai_motion_prompt(motion_prompt, style)
        wave_delay = max(0, int(getattr(scene, "myaddon_ai_enemy_wave_delay", 90)))
        rng = random.Random(seed)
        provider = getattr(scene, "myaddon_ai_enemy_provider", 'BUILTIN')

        if provider == 'GEMINI':
            try:
                plan_data = _request_gemini_enemy_plan(
                    scene,
                    count,
                    seed,
                    style,
                    motion_prompt,
                    wave_delay,
                    center,
                    extents,
                    player,
                )
                blueprints = _sanitize_gemini_enemy_plan(plan_data, count, center, extents, player)
                if getattr(scene, "myaddon_ai_enemy_clear_existing", True):
                    _delete_ai_generated_objects(scene)
                generated_objects, generated_enemies = _create_ai_enemy_objects_from_blueprints(
                    scene,
                    collection,
                    blueprints,
                    motion_prompt,
                    player,
                    "GEMINI",
                )

                bpy.ops.object.select_all(action='DESELECT')
                for obj in generated_objects:
                    obj.select_set(True)
                if generated_enemies:
                    context.view_layer.objects.active = generated_enemies[0]

                errors, warnings = validation.validate_and_store(scene)
                _auto_export_scene_json()

                if errors:
                    self.report({'WARNING'}, f"Geminiで生成しましたが、チェックエラーが{len(errors)}件あります。")
                elif warnings:
                    self.report({'WARNING'}, f"Geminiで生成しました。警告{len(warnings)}件があります。")
                else:
                    self.report({'INFO'}, f"Geminiで敵プランを生成しました: 敵{len(generated_enemies)} / パス{len(generated_enemies)}")
                return {'FINISHED'}
            except Exception as exc:
                if not getattr(scene, "myaddon_ai_enemy_gemini_fallback", True):
                    self.report({'ERROR'}, f"Gemini生成に失敗しました: {exc}")
                    return {'CANCELLED'}
                self.report({'WARNING'}, f"Gemini生成に失敗したため内蔵AIで生成します: {exc}")

        lane_count = max(1, (count + 1) // 2)
        formation_offsets = _build_ai_formation_offsets(count, extents, motion)
        speed_by_style = {
            'BALANCED': 0.050,
            'AMBUSH': 0.060,
            'SWARM': 0.072,
            'PATROL': 0.038,
        }
        generated_objects = []
        generated_enemies = []
        opener_count = min(count, 4 if style == 'SWARM' else 2)
        if motion.get("opener") == "ALL":
            opener_count = count
        elif motion.get("opener") == "ONE":
            opener_count = 1
        margin = Vector((2.0, 2.0, 1.2))
        if getattr(scene, "myaddon_ai_enemy_clear_existing", True):
            _delete_ai_generated_objects(scene)

        for index in range(count):
            pair_index = index // 2
            lane_t = 0.0 if lane_count <= 1 else pair_index / (lane_count - 1)
            side = -1.0 if index % 2 == 0 else 1.0
            z_span = min(extents.z * 0.32, 4.0)
            height = rng.uniform(-z_span, z_span)

            if style == 'AMBUSH':
                lateral = side * extents.x * rng.uniform(0.55, 0.82)
                depth = extents.y * rng.uniform(-0.05, 0.42)
            elif style == 'SWARM':
                swarm_step = min(2.2, extents.x * 0.12)
                lateral = (index - (count - 1) * 0.5) * swarm_step + rng.uniform(-0.7, 0.7)
                depth = extents.y * rng.uniform(0.18, 0.34)
            elif style == 'PATROL':
                lateral = side * extents.x * (0.28 + 0.38 * lane_t)
                depth = extents.y * (0.02 + 0.34 * lane_t)
            else:
                lateral = side * extents.x * (0.22 + 0.45 * lane_t)
                depth = extents.y * (0.22 + 0.28 * (1.0 - lane_t))

            spawn = (
                center
                + player_forward * depth
                + player_right * lateral
                + Vector((0.0, 0.0, height))
            )
            spawn = _clamp_point_to_box(spawn, center, extents, margin)
            formation_offset = formation_offsets[index] if index < len(formation_offsets) else Vector((0.0, 0.0, 0.0))
            if motion.get("keep_formation") and motion.get("pattern") != "EDGE_ORBIT":
                spawn = _clamp_point_to_box(spawn + formation_offset, center, extents, margin)

            path_id = _unique_enemy_path_id(scene)
            speed_jitter = 1.0 if motion.get("keep_formation") else rng.uniform(0.88, 1.12)
            speed = speed_by_style.get(style, 0.050) * motion.get("speed_multiplier", 1.0) * speed_jitter
            loop = motion["loop"] if motion.get("loop") is not None else style == 'PATROL'
            points = _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, pair_index, rng, formation_offset)
            if motion.get("pattern") == "EDGE_ORBIT" and points:
                spawn = points[0]
            handle_type = 'VECTOR' if motion.get("pattern") == "EDGE_ORBIT" and motion.get("respect_bounds") else 'AUTO'
            path_obj = _create_ai_enemy_path(collection, path_id, points, loop, speed, handle_type)
            path_obj["myaddon_ai_motion_prompt"] = motion_prompt

            trigger_name = ""
            delay_frames = wave_delay
            if index >= opener_count and generated_enemies:
                trigger_enemy = generated_enemies[index - opener_count]
                trigger_name = trigger_enemy.name
                delay_jitter = wave_delay // 5
                delay_frames = max(0, wave_delay + rng.randint(-delay_jitter, delay_jitter))

            enemy_name = f"AIEnemy_{index + 1:02d}"
            enemy_type = "VF1-1" if index % 3 == 2 else "VF1"
            enemy_obj = _create_ai_enemy_spawn(
                collection,
                enemy_name,
                spawn,
                player - spawn,
                enemy_type,
                path_id,
                trigger_name,
                delay_frames,
            )
            enemy_obj["myaddon_ai_motion_prompt"] = motion_prompt

            generated_objects.extend([path_obj, enemy_obj])
            generated_enemies.append(enemy_obj)

        bpy.ops.object.select_all(action='DESELECT')
        for obj in generated_objects:
            obj.select_set(True)
        if generated_enemies:
            context.view_layer.objects.active = generated_enemies[0]

        errors, warnings = validation.validate_and_store(scene)
        _auto_export_scene_json()

        if errors:
            self.report({'WARNING'}, f"AI敵プランを生成しましたが、チェックエラーが{len(errors)}件あります。")
        elif warnings:
            self.report({'WARNING'}, f"AI敵プランを生成しました。警告{len(warnings)}件があります。")
        else:
            self.report({'INFO'}, f"AI敵プランを生成しました: 敵{len(generated_enemies)} / パス{len(generated_enemies)}")

        return {'FINISHED'}


class MYADDON_OT_create_enemy_path(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_enemy_path"
    bl_label = "敵飛行パスを配置"
    bl_description = "敵機が追従するスプラインパスを作ります"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        scene = context.scene
        active_enemy = context.active_object if context.active_object and getattr(context.active_object, "game_obj_type", "NONE") == "ENEMY" else None
        path_id = _unique_enemy_path_id(scene)

        curve = bpy.data.curves.new(path_id, 'CURVE')
        curve.dimensions = '3D'
        curve.resolution_u = 24
        curve.bevel_depth = 0.04
        curve.bevel_resolution = 2

        spline = curve.splines.new('BEZIER')
        spline.bezier_points.add(3)

        base = active_enemy.location if active_enemy else Vector((0.0, 0.0, 0.0))
        offsets = [
            Vector((0.0, 0.0, 0.0)),
            Vector((0.0, 5.0, 2.0)),
            Vector((3.0, 10.0, 4.0)),
            Vector((0.0, 15.0, 6.0)),
        ]

        for point, offset in zip(spline.bezier_points, offsets):
            point.co = base + offset
            point.handle_left_type = 'AUTO'
            point.handle_right_type = 'AUTO'

        obj = bpy.data.objects.new(path_id, curve)
        context.collection.objects.link(obj)
        obj.enemy_path_id = path_id
        obj.enemy_path_loop = False
        obj.enemy_path_speed = 0.05

        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        context.view_layer.objects.active = obj

        if active_enemy:
            active_enemy.enemy_path_id = path_id

        print(f"敵飛行パスを配置しました: {path_id}")
        return {'FINISHED'}


class MYADDON_OT_assign_selected_enemy_path(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_assign_selected_enemy_path"
    bl_label = "選択パスを敵に割り当て"
    bl_description = "選択中の敵リスポーン地点へ、同時選択しているパスを割り当てます"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        active = context.active_object
        if not active or getattr(active, "game_obj_type", "NONE") != "ENEMY":
            self.report({'ERROR'}, "割り当て先の敵リスポーン地点をアクティブにしてください。")
            return {'CANCELLED'}

        selected_paths = [
            obj for obj in context.selected_objects
            if obj != active and _is_enemy_path_object(obj)
        ]

        if not selected_paths:
            self.report({'ERROR'}, "割り当てる EnemyPath カーブも一緒に選択してください。")
            return {'CANCELLED'}

        active.enemy_path_id = _get_enemy_path_id(selected_paths[0])
        self.report({'INFO'}, f"{active.name} に {active.enemy_path_id} を割り当てました。")
        return {'FINISHED'}


class MYADDON_OT_assign_selected_reinforcement_trigger(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_assign_selected_reinforcement_trigger"
    bl_label = "選択敵を増援トリガーに設定"
    bl_description = "アクティブな敵リスポーン地点を、同時選択した別の敵が倒された後に出現させます"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        active = context.active_object
        if not active or getattr(active, "game_obj_type", "NONE") != "ENEMY":
            self.report({'ERROR'}, "増援として出現させる敵リスポーン地点をアクティブにしてください。")
            return {'CANCELLED'}

        selected_triggers = [
            obj for obj in context.selected_objects
            if obj != active and getattr(obj, "game_obj_type", "NONE") == "ENEMY"
        ]

        if not selected_triggers:
            self.report({'ERROR'}, "撃破トリガーにする別の敵リスポーン地点も一緒に選択してください。")
            return {'CANCELLED'}

        active.enemy_reinforcement_trigger_name = selected_triggers[0].name
        if getattr(active, "enemy_reinforcement_delay_frames", 0) < 0:
            active.enemy_reinforcement_delay_frames = 0

        validation.validate_and_store(context.scene)
        self.report({'INFO'}, f"{selected_triggers[0].name} が倒されたら {active.name} が出現します。")
        return {'FINISHED'}


class MYADDON_OT_create_stage_bounds(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_stage_bounds"
    bl_label = "ステージ範囲を配置"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_cube_add(size=2.0)
        obj = context.active_object
        obj.name = "StageBounds"
        obj.scale = (10.0, 10.0, 10.0)
        obj.game_obj_type = 'OBSTACLE'
        obj.display_type = 'WIRE'
        print("ワイヤーフレームのステージ範囲を配置しました。")
        return {'FINISHED'}


class MYADDON_OT_create_spawn_point(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_spawn_point"
    bl_label = "スポーン配置"
    bl_options = {'REGISTER', 'UNDO'}

    spawn_type: bpy.props.StringProperty(default="PLAYER")

    def execute(self, context):
        bpy.ops.object.empty_add(type='SINGLE_ARROW', radius=2.0)
        obj = context.active_object

        if self.spawn_type == "PLAYER":
            obj.name = "PlayerSpawn"
            obj.game_obj_type = 'PLAYER'
            print("自機のスポーン地点を配置しました。")
        else:
            obj.name = "EnemyRespawn"
            obj.game_obj_type = 'ENEMY'
            print("敵のリスポーン地点を配置しました。")

        return {'FINISHED'}


classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
    MYADDON_OT_validate_scene,
    MYADDON_OT_playtest_game,
    MYADDON_OT_apply_active_properties_to_selection,
    MYADDON_OT_ai_generate_enemy_plan,
    MYADDON_OT_create_enemy_path,
    MYADDON_OT_assign_selected_enemy_path,
    MYADDON_OT_assign_selected_reinforcement_trigger,
    MYADDON_OT_create_stage_bounds,
    MYADDON_OT_create_spawn_point,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    _unregister_auto_export_handler()
    bpy.app.handlers.save_post.append(_auto_export_assets_on_save)


def unregister():
    _unregister_auto_export_handler()
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
