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
    if os.path.basename(blend_dir) == "level_editor":
        return os.path.abspath(os.path.join(blend_dir, "..", "scene.json"))
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
    if obj.name == "OhirayamaTerrain":
        return False


    try:
        return obj.visible_get()
    except Exception:
        return not obj.hide_get()


def _external_model_file(obj):
    model_file = str(getattr(obj, "game_model_file", "")).strip()
    if model_file:
        return model_file.replace("\\", "/")
    return ""


def _is_individual_model_export_target(obj):
    if not _is_obj_export_target(obj):
        return False
    if obj.name.startswith("StageBounds"):
        return False
    if _external_model_file(obj):
        return False

    category = obj.get("game_obj_type", "NONE")
    return category != "ENEMY"


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

    # GfX|[n_ƂĎgꍇ́A_ł͂Ȃڂ̒S𑗂B
    if obj.type == "MESH" and obj.get("game_obj_type", "NONE") == "ENEMY":
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
        obj.get("enemy_path_id", "None")
        for obj in scene.objects
        if obj.get("enemy_path_id", "None") != "None"
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

def _save_ai_enemy_snapshot(scene):
    import json
    snapshot = []
    for obj in scene.objects:
        if not obj.get(_AI_GENERATED_MARKER):
            continue
        data = {
            "name": obj.name,
            "type": obj.type,
            "location": list(obj.location),
            "rotation_euler": list(obj.rotation_euler),
            "scale": list(obj.scale),
            "empty_display_type": obj.empty_display_type if obj.type == 'EMPTY' else None,
            "empty_display_size": obj.empty_display_size if obj.type == 'EMPTY' else None,
            "show_name": obj.show_name if obj.type == 'EMPTY' else False,
            "game_obj_type": obj.get("game_obj_type"),
            "enemy_type": obj.get("enemy_type"),
            "enemy_path_id": obj.get("enemy_path_id"),
            "enemy_reinforcement_trigger_name": obj.get("enemy_reinforcement_trigger_name"),
            "enemy_reinforcement_delay_frames": obj.get("enemy_reinforcement_delay_frames"),
            "enemy_path_loop": obj.get("enemy_path_loop"),
            "enemy_path_speed": obj.get("enemy_path_speed"),
        }
        if obj.type == 'CURVE' and obj.data.splines:
            spline = obj.data.splines[0]
            data["points"] = [list(bp.co) for bp in spline.bezier_points]
        snapshot.append(data)
    return json.dumps(snapshot)

def _restore_ai_enemy_snapshot(scene, snapshot_json):
    if not snapshot_json: return
    try:
        import json
        snapshot = json.loads(snapshot_json)
    except:
        return
    _delete_ai_generated_objects(scene)
    collection = _get_ai_collection(scene)
    for data in snapshot:
        obj = None
        if data["type"] == 'EMPTY':
            obj = bpy.data.objects.new(data["name"], None)
            if data.get("empty_display_type"):
                obj.empty_display_type = data["empty_display_type"]
            if data.get("empty_display_size"):
                obj.empty_display_size = data["empty_display_size"]
            if data.get("show_name"):
                obj.show_name = data["show_name"]
        elif data["type"] == 'CURVE':
            curve = bpy.data.curves.new(data["name"] + "Curve", type='CURVE')
            curve.dimensions = '3D'
            spline = curve.splines.new('BEZIER')
            points = data.get("points", [])
            if len(points) > 1:
                spline.bezier_points.add(len(points) - 1)
            for i, co in enumerate(points):
                bp = spline.bezier_points[i]
                bp.co = co
                bp.handle_left_type = 'AUTO'
                bp.handle_right_type = 'AUTO'
            obj = bpy.data.objects.new(data["name"], curve)
        else:
            continue
        collection.objects.link(obj)
        obj.location = data["location"]
        obj.rotation_euler = data["rotation_euler"]
        obj.scale = data["scale"]
        obj[_AI_GENERATED_MARKER] = True
        
        def _set_prop(key):
            if data.get(key) is not None:
                obj[key] = data[key]
                
        _set_prop("game_obj_type")
        _set_prop("enemy_type")
        _set_prop("enemy_path_id")
        _set_prop("enemy_reinforcement_trigger_name")
        _set_prop("enemy_reinforcement_delay_frames")
        _set_prop("enemy_path_loop")
        _set_prop("enemy_path_speed")


def _get_true_bounds(obj):
    from mathutils import Vector
    bbox_corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    min_x = min([v.x for v in bbox_corners])
    max_x = max([v.x for v in bbox_corners])
    min_y = min([v.y for v in bbox_corners])
    max_y = max([v.y for v in bbox_corners])
    min_z = min([v.z for v in bbox_corners])
    max_z = max([v.z for v in bbox_corners])
    center = Vector(((min_x + max_x) / 2.0, (min_y + max_y) / 2.0, (min_z + max_z) / 2.0))
    extents = Vector(((max_x - min_x) / 2.0, (max_y - min_y) / 2.0, (max_z - min_z) / 2.0))
    return center, extents

def _stage_bounds_box(scene):
    from mathutils import Vector
    stage_bounds = next((obj for obj in scene.objects if obj.name.startswith("StageBounds")), None)
    if stage_bounds:
        center, extents = _get_true_bounds(stage_bounds)
        extents.x = max(1.0, extents.x)
        extents.y = max(1.0, extents.y)
        extents.z = max(1.0, extents.z)
        return center, extents

    return Vector((0.0, 0.0, 0.0)), Vector((18.0, 22.0, 10.0))

def _get_target_bounds(scene, prompt=""):
    from mathutils import Vector
    if prompt:
        sorted_objs = sorted([o for o in scene.objects if o.type == 'MESH'], key=lambda o: len(o.name), reverse=True)
        for obj in sorted_objs:
            if obj.name in prompt:
                if obj.name == "StageBounds":
                    break
                center, extents = _get_true_bounds(obj)
                extents.x = max(extents.x + 2.0, 5.0)
                extents.y = max(extents.y + 2.0, 5.0)
                extents.z = max(extents.z + 2.0, 5.0)
                return center, extents
                
    try:
        from . import operators
        return operators._stage_bounds_box(scene)
    except:
        try:
            return _stage_bounds_box(scene)
        except:
            return Vector((0,0,0)), Vector((20,20,20))


def _find_player_spawn_position(scene, fallback_center, fallback_extents):
    for obj in scene.objects:
        if obj.get("game_obj_type", "NONE") == "PLAYER":
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

def _get_learning_file_path():
    import os
    return os.path.join(os.path.dirname(__file__), "ai_learning.json")

def _load_ai_learning():
    import os
    import json
    path = _get_learning_file_path()
    if os.path.exists(path):
        try:
            with open(path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception:
            pass
    return {
        "level_style_weights": {},
        "enemy_style_weights": {}
    }

def _save_ai_learning(data):
    import json
    path = _get_learning_file_path()
    try:
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
    except Exception as e:
        print(f"Failed to save ai_learning.json: {e}")

def _is_prompt_complex(prompt_text):
    if not prompt_text:
        return False
    text = str(prompt_text).strip()
    return len(text) > 0


def _parse_ai_enemy_prompt(prompt_text):
    normalized = str(prompt_text or "").casefold()
    matched_any = False

    def check(*kws):
        nonlocal matched_any
        if _prompt_has(normalized, *kws):
            matched_any = True
            return True
        return False

    
    style = 'BALANCED'
    if check( "ばらつ", "ばらつけ", "散ら", "バラバラ", "散布", "分散", "集まらない", "中心に集まらない", "中心に集まら", "広範囲", "広げる", "全域", "scatter", "spread", "disperse", "random"):
        style = 'SCATTER'
    elif check( "挟み撃ち", "背後", "回り込", "ambush"):
        style = 'AMBUSH'
    elif check( "群れ", "集団", "一団", "大量", "swarm"):
        style = 'SWARM'
    elif check( "巡回", "パトロール", "patrol"):
        style = 'PATROL'
        
    motion = {
        "matched_keywords": False,
        "pattern": "DEFAULT",
        "vertical": "NONE",
        "turn_sign": 1.0,
        "loop": True,
        "speed_multiplier": 1.0,
        "amplitude_multiplier": 0.45,
        "length_multiplier": 1.0,
        "opener": None,
        "keep_formation": False,
        "respect_bounds": True,
        "edge_margin": 2.0,
        "enemy_type": None,
        "enemy_path_id": None,
    }
    
    if check( "vf3", "vf-3", "地上"):
        motion["enemy_type"] = "VF3"
    elif check( "vf1-1", "vf-1-1"):
        motion["enemy_type"] = "VF1-1"
    elif check( "vf1", "vf-1"):
        motion["enemy_type"] = "VF1"

    if check( "反時計", "左回り", "counter", "ccw"):
        motion["turn_sign"] = -1.0
    elif check( "時計", "右回り", "clockwise", "cw"):
        motion["turn_sign"] = 1.0

    wants_edge = check( "フィールド", "外周", "端", "端沿い", "ぎりぎり", "ギリギリ", "境界", "枠", "場外", "edge", "border", "boundary")
    wants_orbit = check( "回る", "旋回", "円", "一周", "一週", "1周", "1週", "周回", "巡回", "ループ", "orbit", "circle", "loop")
    if check( "群れ", "隊列", "編隊", "フォーメーション", "まとま", "崩さ", "一団", "formation", "swarm"):
        motion["keep_formation"] = True
        motion["opener"] = "ALL"
    if check( "外に出ない", "外へ出ない", "はみ出", "出ない", "収め", "範囲内", "inside", "bounds"):
        motion["respect_bounds"] = True
    if check( "ぎりぎり", "ギリギリ", "端沿い", "外周", "境界", "edge", "border"):
        motion["edge_margin"] = 1.0

    if check( "板野サーカス", "マクロス", "macross"):
        motion["pattern"] = "SPIRAL"
        motion["speed_multiplier"] = 1.8
        motion["amplitude_multiplier"] = 1.5
    elif check( "ファンネル", "ドラグーン", "funnel", "dragoon"):
        motion["pattern"] = "ORBIT"
        motion["keep_formation"] = False
        motion["opener"] = "ALL"
        motion["speed_multiplier"] = 1.5
        motion["amplitude_multiplier"] = 1.3
    elif check( "トランザム", "ゼロシステム", "trans-am", "transam"):
        motion["pattern"] = "ZIGZAG"
        motion["speed_multiplier"] = 2.0
        motion["amplitude_multiplier"] = 1.4
    elif check( "超スピード", "神速", "超光速"):
        motion["speed_multiplier"] = 2.5

    prevent_loop = check( "突撃", "直線", "まっすぐ", "正面", "接近", "ループさせない", "ループしない", "ループなし", "ループさせず", "rush", "charge", "straight", "no loop")

    if prevent_loop:
        motion["pattern"] = "STRAIGHT"
        motion["loop"] = False
    elif wants_edge and wants_orbit:
        motion["pattern"] = "EDGE_ORBIT"
    elif check( "螺旋", "スパイラル", "spiral"):
        motion["pattern"] = "SPIRAL"
        motion["amplitude_multiplier"] = max(motion["amplitude_multiplier"], 1.15)
    elif check( "ジグザグ", "蛇行", "左右", "s字", "s-字", "zigzag", "zig-zag"):
        motion["pattern"] = "ZIGZAG"
        motion["amplitude_multiplier"] = max(motion["amplitude_multiplier"], 1.25)
    elif wants_orbit:
        motion["pattern"] = "ORBIT"
        
    if check( "入り組んだ", "複雑", "激しく", "intricate", "complex"):
        motion["pattern"] = "ZIGZAG"
        motion["amplitude_multiplier"] = max(motion["amplitude_multiplier"], 1.5)
        motion["length_multiplier"] = min(motion["length_multiplier"], 0.4)

    if check( "短", "小さ", "狭", "short", "small", "tiny"):
        motion["amplitude_multiplier"] = min(motion["amplitude_multiplier"], 0.5)
        motion["length_multiplier"] = 0.35
    elif check( "大き", "広", "でか", "big", "large", "wide", "huge"):
        motion["amplitude_multiplier"] = 2.0
        motion["length_multiplier"] = 2.0

    if check( "上昇", "上に", "上が", "登", "高く", "climb", "up"):
        motion["vertical"] = "UP"
    elif check( "下降", "下に", "下が", "降下", "低く", "dive", "down"):
        motion["vertical"] = "DOWN"
    elif check( "上下", "波", "ウェーブ", "wave", "up and down"):
        motion["vertical"] = "WAVE"
    elif check( "急降下", "ダイブ", "dive", "drop"):
        motion["vertical"] = "DIVE"

    if check( "速", "猛", "ダッシュ", "fast", "speed", "quick", "rapid") and motion["speed_multiplier"] == 1.0:
        motion["speed_multiplier"] = 1.6
    elif check( "遅", "ゆっくり", "緩やか", "slow"):
        motion["speed_multiplier"] = 0.5
        
    import re
    path_match = re.search(r'(path_\w+)', normalized)
    if path_match:
        motion["enemy_path_id"] = path_match.group(1)
        
    motion["matched_keywords"] = matched_any
    return style, motion


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

def _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, pair_index, rng, formation_offset=None):
    if formation_offset is None:
        formation_offset = Vector((0.0, 0.0, 0.0))

    margin = Vector((1.5, 1.5, 1.0))
    to_player = player - spawn
    flat_to_player = Vector((to_player.x, to_player.y, 0.0))
    forward = _safe_normalized(flat_to_player, Vector((0.0, -1.0, 0.0)))
    right = Vector((forward.y, -forward.x, 0.0))
    amplitude = float(motion.get("amplitude_multiplier", 1.0))
    length_mult = float(motion.get("length_multiplier", 1.0))
    turn_side = side * float(motion.get("turn_sign", 1.0))
    lateral = min(extents.x * 0.45, 8.0) * side * amplitude
    z_swing = min(extents.z * 0.18, 2.5) * amplitude
    pattern = motion.get("pattern", "DEFAULT")

    if pattern == "EDGE_ORBIT":
        raw_points = _build_edge_orbit_points(motion, center, extents, formation_offset)
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        edge_margin = float(motion.get("edge_margin", 2.0))
        return [_clamp_point_to_box(point, center, extents, Vector((edge_margin, edge_margin, 1.0))) for point in raw_points]

    if pattern == "SPIRAL":
        focus = player + forward * min(extents.y * 0.12, 3.0) * length_mult
        radius_x = min(extents.x * (0.22 + pair_index * 0.025), 7.5) * amplitude
        radius_y = min(extents.y * (0.16 + pair_index * 0.025), 7.5) * amplitude * length_mult
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
        focus = player + forward * min(extents.y * 0.08, 2.5) * length_mult
        radius_x = min(extents.x * (0.24 + pair_index * 0.03), 8.0) * amplitude
        radius_y = min(extents.y * (0.18 + pair_index * 0.025), 8.0) * amplitude * length_mult
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
        step = min(max((player - spawn).length / 4.0, 3.0), extents.y * 0.28) * length_mult
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
        overshoot = min(extents.y * 0.24, 6.0) * length_mult
        raw_points = [
            spawn,
            spawn + forward * min(extents.y * 0.24, 6.0) * length_mult + Vector((0.0, 0.0, z_swing * 0.25)),
            player + forward * overshoot,
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if style == 'PATROL':
        radius_x = min(extents.x * 0.25 + pair_index * 0.4, 7.0) * amplitude
        radius_y = min(extents.y * 0.18 + pair_index * 0.3, 8.0) * amplitude
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
            player - forward * min(extents.y * 0.1, 2.0) - right * lateral + Vector((0.0, 0.0, -z_swing)),
            spawn + right * (lateral * 0.5) + forward * min(extents.y * 0.1, 2.0),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if style == 'AMBUSH':
        lateral *= 1.15
        raw_points = [
            spawn,
            center + right * lateral + Vector((0.0, 0.0, z_swing)),
            player - right * (lateral * 0.65) + Vector((0.0, 0.0, -z_swing)),
            spawn + right * (lateral * 0.5) + forward * min(extents.y * 0.15, 3.0),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    raw_points = [
        spawn,
        center + right * lateral + Vector((0.0, 0.0, z_swing)),
        player + forward * min(extents.y * 0.2, 6.0) - right * (lateral * 0.45),
        spawn + right * (lateral * 0.5) + forward * min(extents.y * 0.15, 3.0),
    ]
    raw_points = _apply_vertical_motion(raw_points, motion, extents)
    return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]


def _ai_enemy_plan_schema():
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
                            "enum": ["AUTO", "VECTOR", "ALIGNED", "FREE"],
                        },
                    },
                    "required": ["spawn", "path", "loop", "speed", "enemy_type", "trigger_index", "delay_frames", "handle_type"],
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


def _coerce_ai_point(value):
    import mathutils
    if isinstance(value, dict) and "x" in value and "y" in value and "z" in value:
        return mathutils.Vector((float(value["x"]), float(value["y"]), float(value["z"])))
    if isinstance(value, list) and len(value) >= 3:
        return mathutils.Vector((float(value[0]), float(value[1]), float(value[2])))
    return None

def _sanitize_enemy_plan_data(plan_data, count, center, extents, player):
    import bpy
    import mathutils
    from mathutils import Vector
    scene = bpy.context.scene
    enemies = plan_data.get("enemies") if isinstance(plan_data, dict) else None
    if not isinstance(enemies, list) or not enemies:
        raise ValueError("AIから敵データが返ってきませんでした。")

    blueprints = []
    margin = Vector((0.5, 0.5, 0.5))
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

        enemy_type_check = str(enemy.get("enemy_type", "")).strip().upper().replace("-", "").replace("_", "").replace(" ", "")
        is_ground_blueprint = ("VF3" in enemy_type_check or "GROUND" in enemy_type_check or "TANK" in enemy_type_check or "LAND" in enemy_type_check)

        if not is_ground_blueprint:
            points = _sanitize_path_distances(points)
            if len(points) < 2 or all((point - spawn).length < 0.05 for point in points[1:]):
                offset = Vector((1.0 if spawn.x < center.x else -1.0, 0, 0))
                points = [spawn, _clamp_point_to_box(spawn + offset, center, extents, margin)]
        else:
            points = [spawn]

        loop = bool(enemy.get("loop", False))
        speed = float(enemy.get("speed", 0.05))
        if speed <= 0 or speed > 0.5:
            speed = 0.05
            
        handle_type = str(enemy.get("handle_type", "AUTO")).upper()
        if handle_type not in {'AUTO', 'VECTOR', 'FREE', 'ALIGNED'}:
            handle_type = 'AUTO'
            
        enemy_type = str(enemy.get("enemy_type", "")).strip()
        if not enemy_type:
            enemy_type = getattr(scene, "myaddon_ai_enemy_base_type", "VF1")
            
        trigger_index = enemy.get("trigger_index", -1)
        if trigger_index is not None:
            try:
                trigger_index = int(trigger_index)
            except ValueError:
                trigger_index = -1
                
        delay_frames = enemy.get("delay_frames")
        if delay_frames is not None:
            try:
                delay_frames = int(delay_frames)
            except ValueError:
                delay_frames = getattr(scene, "myaddon_ai_enemy_wave_delay", 90)
        else:
            delay_frames = getattr(scene, "myaddon_ai_enemy_wave_delay", 90)

        blueprint = {
            "spawn": spawn,
            "path": points,
            "loop": loop,
            "speed": speed,
            "handle_type": handle_type,
            "enemy_type": enemy_type,
            "trigger_index": trigger_index,
            "delay_frames": delay_frames,
            "shape_override": enemy.get("shape_override"),
            "size_reference": enemy.get("size_reference")
        }
        
        path_id = str(enemy.get("enemy_path_id", "")).strip()
        if not path_id:
            path_id = getattr(scene, "myaddon_ai_enemy_base_path_id", "")
        if path_id:
            blueprint["enemy_path_id"] = path_id
            
        blueprints.append(blueprint)
        
    return blueprints

def _stage_bounds_prompt(center, extents):
    return {
        "center": [center.x, center.y, center.z],
        "extents": [extents.x, extents.y, extents.z],
        "min": [center.x - extents.x, center.y - extents.y, center.z - extents.z],
        "max": [center.x + extents.x, center.y + extents.y, center.z + extents.z],
    }


def _build_gemini_enemy_prompt(count, seed, style, motion_prompt, wave_delay, center, extents, player):
    style_label = {
        'BALANCED': "oX",
        'AMBUSH': "݌",
        'SWARM': "Q",
        'PATROL': "",
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
        "- Every spawn and path point must stay inside the field bounds. DO NOT cluster points near the center.\n"
        "- Actively use the entire field bounds (min to max). Scatter enemies dynamically!\n"
        "- If requested to place at edge/corner/far side, use coordinates near bounds min/max limits.\n"
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


def _request_ollama_enemy_plan(scene, count, seed, style, motion_prompt, wave_delay, center, extents, player):
    api_key = _resolve_gemini_api_key(scene)
    if not api_key:
        raise ValueError("Gemini APIキーが設定されていません。")

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
            "responseSchema": _ai_enemy_plan_schema(),
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

    timeout = max(5, int(getattr(scene, "myaddon_ai_enemy_ollama_timeout", 25)))
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            response_data = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"Gemini APIエラー {exc.code}: {detail[:280]}") from exc
    except urllib.error.URLError as exc:
        raise RuntimeError(f"Gemini API接続に失敗しました: {exc.reason}") from exc

    candidates = response_data.get("candidates", [])
    if not candidates:
        raise RuntimeError("Geminiからデータが返ってきませんでした。")

    parts = candidates[0].get("content", {}).get("parts", [])
    text = "".join(part.get("text", "") for part in parts)
    if not text.strip():
        raise RuntimeError("Gemini̕ԓłB")
    return _parse_gemini_json_text(text)



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



def _ai_enemy_plan_schema():
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
                        "loop": {"type": "boolean"},
                        "speed": {"type": "number"},
                        "enemy_type": {
                            "type": "string"
                        },
                        "enemy_path_id": {
                            "type": "string"
                        },
                        "shape_override": {
                            "type": "string",
                            "enum": ["CIRCLE", "OVAL", "FIGURE8", "ZIGZAG", "SWEEP", "EDGE_ORBIT", "ORBIT", "SPIRAL", "STRAIGHT", "DEFAULT"]
                        },
                        "size_reference": {
                            "type": "string"
                        },
                        "trigger_index": {"type": "integer"},
                        "delay_frames": {"type": "integer"},
                        "handle_type": {
                            "type": "string",
                            "enum": ["AUTO", "VECTOR", "ALIGNED", "FREE"],
                        },
                    },
                    "required": ["spawn", "path", "loop", "speed", "enemy_type", "trigger_index", "delay_frames", "handle_type"],
                },
            },
            "notes": {"type": "string"},
        },
        "required": ["enemies"],
    }


def _resolve_gemini_api_key(context):
    window_manager = getattr(context, "window_manager", None)
    if window_manager:
        session_key = getattr(window_manager, "myaddon_ai_enemy_gemini_api_key", "").strip()
        if session_key:
            return session_key
    return os.environ.get("GEMINI_API_KEY", "").strip()


def _get_scene_objects_summary(scene):
    if not scene:
        return []
    summary = []
    for obj in scene.objects:
        if obj.type not in ['MESH', 'CURVE']:
            continue
        if obj.name.startswith("."):
            continue
        dims = obj.dimensions
        summary.append({
            "name": obj.name,
            "type": obj.type,
            "location": {"x": round(obj.location.x, 2), "y": round(obj.location.y, 2), "z": round(obj.location.z, 2)},
            "size": {"x": round(dims.x, 2), "y": round(dims.y, 2), "z": round(dims.z, 2)}
        })
    return summary


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


def _build_gemini_enemy_prompt(count, seed, history_msgs, wave_delay, center, extents, player, baseline_plan=None, scene=None):
    bounds = _stage_bounds_prompt(center, extents)
    player_data = {"x": player.x, "y": player.y, "z": player.z}
    
    prompt = (
        "You are an AI level designer assistant for a 3D side-scrolling action game.\n"
        "Based on the conversation history below, determine the final enemy spawn plan.\n"
        "Return ONLY valid JSON matching the schema.\n"
        "Coordinate system: X is left/right, Y is field depth, Z is height. Unit is Blender world unit.\n"
        f"Enemy count MUST be exactly {count} unless the user explicitly requested a different amount.\n"
        f"Seed hint: {seed}. Default reinforcement delay: {wave_delay} frames.\n"
        f"Field bounds JSON: {json.dumps(bounds, ensure_ascii=False)}\n"
        f"Player spawn JSON: {json.dumps(player_data, ensure_ascii=False)}\n"
    )
    
    if scene:
        summary = _get_scene_objects_summary(scene)
        prompt += f"Existing Scene Objects Summary (JSON) - Use these sizes/locations as reference if requested:\n{json.dumps(summary, ensure_ascii=False)}\n"
        
    prompt += (
        "Rules:\n"
        "- Every spawn and path point must stay inside the field bounds. DO NOT cluster points near the center.\n"
        "- Actively use the entire field bounds (min to max). Scatter enemies dynamically!\n"
        "- If requested to place at edge/corner/far side, use coordinates near bounds min/max limits.\n"
        "- If `enemy_path_id` is provided by the user (e.g. 'Path_001'), specify it. If specified, the generated `path` points might be ignored, but provide a single dummy point in `path` to satisfy schema.\n"
        "- If `enemy_path_id` is empty, generate 3 to 10 points for `path`. Unless a standard shape is requested.\n"
        "- IMPORTANT: If the user asks for a standard shape (circle, zigzag, orbit, etc.), do NOT generate a complex `path`. Instead, set `shape_override` to the corresponding enum (e.g., 'CIRCLE', 'ZIGZAG') and provide a dummy `path`.\n"
        "- IMPORTANT: If the user asks to match the size of an existing object (e.g., 'size of EnemyPath'), set `size_reference` to the exact name of that object from the Existing Scene Objects Summary.\n"
        "- Use handle_type=\"VECTOR\" for paths that must not bulge outside the field.\n"
        "- Use `enemy_type` as requested (e.g. 'VF1-1', 'VF1').\n"
        "- Use `loop=true` for patrol, orbit, circle.\n"
        "- `trigger_index` is -1 for enemies that appear immediately. To make an enemy appear when another is defeated, set `trigger_index` to the array index of that target enemy.\n"
        f"- You MUST output exactly {count} enemies in the JSON array unless specifically requested otherwise.\n"
    )
    if baseline_plan:
        prompt += f"\nBaseline Plan generated by Built-in AI:\n{json.dumps(baseline_plan, ensure_ascii=False)}\n\n"
        prompt += "Instruction: Use the Baseline Plan above as a starting point. Only modify the coordinates or parameters that need to be changed to satisfy the user's specific request. Keep the core elements if they do not conflict.\n"

    prompt += "\nConversation History:\n"
    for msg in history_msgs:
        prompt += f"[{msg.role}] {msg.content}\n"
        
    prompt += "\nNow, output the JSON."
    return prompt


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

def _request_ollama_enemy_plan(context, scene, count, seed, history_msgs, wave_delay, center, extents, player, baseline_plan=None):
    model = getattr(scene, "myaddon_ai_ollama_model", "llama3")
    url = "http://localhost:11434/api/generate"
    
    prompt = _build_gemini_enemy_prompt(count, seed, history_msgs, wave_delay, center, extents, player, baseline_plan, scene=scene)
    prompt += "\n\nJSON Schema:\n" + json.dumps(_ai_enemy_plan_schema(), indent=2)
    prompt += "\n\nYou MUST return only valid JSON matching the exact schema."

    payload = {
        "model": model,
        "prompt": prompt,
        "format": "json",
        "stream": False
    }
    request = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
        },
        method="POST",
    )

    timeout = max(5, int(getattr(scene, "myaddon_ai_enemy_ollama_timeout", 25)))
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            response_data = json.loads(response.read().decode("utf-8"))
            
        response_text = response_data.get("response", "")
        if not response_text.strip():
            raise RuntimeError("Ollamaからの応答が空欄です。")
            
        return _parse_gemini_json_text(response_text)
    except urllib.error.URLError as exc:
        raise RuntimeError(f"Ollamaに接続できません。起動しているか確認してください: {exc.reason}") from exc
    except Exception as exc:
        print(f"Ollama Enemy Plan error: {exc}")
        raise RuntimeError(str(exc))
def _ai_level_plan_schema():
    return {
        "type": "OBJECT",
        "properties": {
            "style": {"type": "STRING", "enum": ["CITY", "DEFENSE_LINE", "MAZE", "ARENA"]},
            "shape": {"type": "STRING", "enum": ["CUBE", "CYLINDER"]},
            "density_multiplier": {"type": "NUMBER"},
            "match_player_size": {"type": "BOOLEAN"},
            "randomize_location": {"type": "BOOLEAN"},
        },
        "required": ["style", "shape", "density_multiplier", "match_player_size", "randomize_location"]
    }

def _build_gemini_level_prompt(history_msgs, scene=None):
    prompt = """You are an AI level designer assistant for a 3D side-scrolling action game.
Based on the conversation history below, determine the final level obstacle generation parameters.

Output MUST be JSON matching the following schema. Do NOT output markdown code blocks.
- style: "CITY" (blocks), "DEFENSE_LINE" (walls/barricades), "MAZE" (random layout), "ARENA" (circular).
- shape: "CUBE" or "CYLINDER".
- density_multiplier: Float. 1.0 is normal. e.g. 2.0 for lots, 0.5 for sparse.
- match_player_size: boolean. Set to true if the user asks for the obstacles to be the same size as the player ("プレイヤーと同じサイズ", etc).
- randomize_location: true for jittering.
"""
    
    if scene:
        summary = _get_scene_objects_summary(scene)
        prompt += f"\nExisting Scene Objects Summary (JSON) - Use these sizes as reference if requested:\n{json.dumps(summary, ensure_ascii=False)}\n"
        
    prompt += "\nConversation History:\n"
    for msg in history_msgs:
        prompt += f"[{msg.role}] {msg.content}\n"
    
    prompt += "\nNow, output the JSON."
    return prompt

def _request_ollama_level_plan(context, scene, history_msgs):
    model = getattr(scene, "myaddon_ai_ollama_model", "llama3")
    url = "http://localhost:11434/api/generate"
    prompt = _build_gemini_level_prompt(history_msgs, scene=scene)
    prompt += "\n\nJSON Schema:\n" + json.dumps(_ai_level_plan_schema(), indent=2)
    prompt += "\n\nYou MUST return only valid JSON matching the exact schema."

    payload = {
        "model": model,
        "prompt": prompt,
        "format": "json",
        "stream": False
    }
    request = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
        },
        method="POST",
    )

    timeout = max(5, int(getattr(scene, "myaddon_ai_enemy_ollama_timeout", 25)))
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            text = response.read().decode("utf-8")
            
        data = json.loads(text)
        response_text = data.get("response", "")
        return _parse_gemini_json_text(response_text)
    except Exception as exc:
        print(f"Ollama Level Plan error: {exc}")
        return None


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



def _path_start_facing(points, fallback):
    start = points[0] if points else Vector((0.0, 0.0, 0.0))
    for point in points[1:]:
        direction = point - start
        if direction.length > 0.05:
            return direction
    return fallback


def _snap_point_to_terrain(point, terrain_objs):
    if not terrain_objs:
        return point.x, point.y, 0.0

    px = float(point.x)
    py = float(point.y)
    depsgraph = bpy.context.evaluated_depsgraph_get()

    highest_z = -99999.0
    found = False

    for t_obj in terrain_objs:
        try:
            mw = t_obj.matrix_world
            inv_mw = mw.inverted()
            l_orig = inv_mw @ Vector((px, py, 5000.0))
            l_dir = inv_mw.to_3x3() @ Vector((0.0, 0.0, -1.0))
            l_dir.normalize()

            hit, loc, norm, _ = t_obj.ray_cast(l_orig, l_dir)
            if hit:
                w_loc = mw @ loc
                if w_loc.z > highest_z:
                    highest_z = w_loc.z
                    found = True
        except:
            pass

    if found and highest_z > -9000.0:
        return px, py, highest_z + 0.35

    closest_dist_sq = 1e9
    closest_z = -99999.0

    for t_obj in terrain_objs:
        try:
            mw = t_obj.matrix_world
            eval_obj = t_obj.evaluated_get(depsgraph)
            mesh = eval_obj.to_mesh()

            for v in mesh.vertices:
                wv = mw @ v.co
                dx = wv.x - px
                dy = wv.y - py
                d_sq = dx * dx + dy * dy
                if d_sq < closest_dist_sq:
                    closest_dist_sq = d_sq
                    closest_z = wv.z

            eval_obj.to_mesh_clear()
        except:
            pass

    if closest_dist_sq < 225.0 and closest_z > -9000.0:
        return px, py, closest_z + 0.35

    min_z = 0.0
    try:
        all_z = []
        for t_obj in terrain_objs:
            mw = t_obj.matrix_world
            for v in t_obj.data.vertices:
                all_z.append((mw @ v.co).z)
        if all_z:
            min_z = min(all_z)
    except:
        pass

    target_z = max(0.0, min_z) + 0.35
    return px, py, target_z


def _create_ai_enemy_objects_from_blueprints(scene, collection, blueprints, motion_prompt, player, provider_label):
    generated_objects = []
    generated_enemies = []

    terrain_objs = [
        obj for obj in scene.objects 
        if obj.type == 'MESH' 
        and obj.name != "StageBounds" 
        and not obj.name.startswith("AIEnemy") 
        and not obj.name.startswith("Enemy")
        and not obj.name.startswith("Camera")
        and not obj.name.startswith("Light")
    ]

    for index, blueprint in enumerate(blueprints):
        spawn = blueprint["spawn"]
        points = blueprint.get("points") or blueprint.get("path")
        enemy_type = str(blueprint.get("enemy_type", "VF3")).strip()

        clean_type = enemy_type.upper().replace("-", "").replace("_", "").replace(" ", "")
        is_ground_type = ("VF3" in clean_type or "GROUND" in clean_type or "TANK" in clean_type or "LAND" in clean_type)

        if is_ground_type and terrain_objs:
            spawn_x, spawn_y, spawn_z = _snap_point_to_terrain(spawn, terrain_objs)
            spawn = Vector((spawn_x, spawn_y, spawn_z))
            points = [spawn]

        enemy_path_id_input = str(blueprint.get("enemy_path_id", "")).strip()
        path_obj = None

        if not is_ground_type:
            if enemy_path_id_input:
                path_id = enemy_path_id_input
            else:
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
        else:
            path_id = ""

        trigger_name = ""
        trigger_index = blueprint.get("trigger_index", -1)
        if 0 <= trigger_index < len(generated_enemies):
            trigger_name = generated_enemies[trigger_index].name
        elif trigger_index == -1:
            ui_trigger_target = getattr(scene, "myaddon_ai_enemy_trigger_target", None)
            if ui_trigger_target:
                trigger_name = ui_trigger_target

        enemy_name = f"AIEnemy_{index + 1:02d}"
        enemy_obj = _create_ai_enemy_spawn(
            collection,
            enemy_name,
            spawn,
            _path_start_facing(points, player - spawn),
            enemy_type,
            path_id,
            trigger_name,
            blueprint.get("delay_frames", 0),
        )
        enemy_obj["myaddon_ai_motion_prompt"] = motion_prompt
        enemy_obj["myaddon_ai_provider"] = provider_label

        if path_obj:
            generated_objects.extend([path_obj, enemy_obj])
        else:
            generated_objects.append(enemy_obj)

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

    external_model = _external_model_file(obj)
    if external_model:
        obj_data["model"] = external_model
    elif obj.type == 'MESH' and model_filenames and obj.name in model_filenames:
        obj_data["model"] = model_filenames[obj.name]

    if hasattr(obj, "enemy_type") and obj.enemy_type != "None":
        obj_data["enemy"] = {
            "type": obj.enemy_type,
        }

    if getattr(obj, "game_obj_type", "NONE") == 'ENEMY':
        path_id = getattr(obj, "enemy_path_id", "None")
        if path_id != "None":
            obj_data["path_id"] = path_id

        wave_num = getattr(obj, "enemy_wave_number", 0)
        trigger_name = getattr(obj, "enemy_reinforcement_trigger_name", "")
        
        if wave_num > 0:
            import bpy
            scene = bpy.context.scene
            prev_wave_enemies = [
                e for e in scene.objects
                if getattr(e, "game_obj_type", "NONE") == 'ENEMY'
                and getattr(e, "enemy_wave_number", 0) == wave_num - 1
            ]
            auto_trigger = ", ".join([e.name for e in prev_wave_enemies])
            
            if auto_trigger:
                if _is_unset_text(trigger_name):
                    trigger_name = auto_trigger
                else:
                    trigger_name = auto_trigger + ", " + trigger_name

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
        return f"G[ {len(errors)} / x {len(warnings)}"
    if warnings:
        return f"x {len(warnings)}"
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
    scene = bpy.context.scene

    valid_path_ids = {
        getattr(o, "enemy_path_id", "None") 
        for o in scene.objects 
        if o.type == 'CURVE' or o.name.startswith("EnemyPath")
    }
    all_enemy_names = {
        o.name for o in scene.objects 
        if getattr(o, "game_obj_type", "NONE") == "ENEMY" or o.name.startswith("AIEnemy")
    }

    for obj in scene.objects:
        if getattr(obj, "game_obj_type", "NONE") == "ENEMY" or obj.name.startswith("AIEnemy"):
            if _is_unset_text(getattr(obj, "enemy_type", None)):
                obj.enemy_type = "VF3"
            path_id = getattr(obj, "enemy_path_id", "None")
            if path_id != "None" and path_id not in valid_path_ids:
                obj.enemy_path_id = "None"
            trig = getattr(obj, "enemy_reinforcement_trigger_name", "")
            if trig and trig not in all_enemy_names:
                obj.enemy_reinforcement_trigger_name = ""

    errors, warnings = validation.validate_scene(scene)

    try:
        _export_scene_to_path(scene, filepath, model_filenames)
        if warnings:
            print(f"scene.json を自動エクスポートしました (警告 {len(warnings)} 件): {filepath}")
        else:
            print(f"scene.json を自動エクスポートしました: {filepath}")
    except Exception as exc:
        print(f"scene.json の自動エクスポートに失敗しました: {exc}")


def _auto_export_obj(model_filenames=None):
    scene = bpy.context.scene
    if model_filenames is None:
        model_filenames = _build_model_filename_map(scene)

    try:
        exported_count = _export_individual_meshes_to_obj(scene, model_filenames)
        if exported_count == 0:
            print("OBJ/MTL o͂XLbv܂B\̃bV܂B")
        else:
            print(f"OBJ/MTLo͂܂i{exported_count}̃bVj")
    except Exception as exc:
        print(f"OBJ/MTL o͂Ɏs܂: {exc}")


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
    bl_label = "_L΂"
    bl_description = "_WĐL΂܂"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.data.objects["Cube"].data.vertices[0].co.x += 1.0
        print("_L΂܂B")
        return {'FINISHED'}


class MYADDON_OT_create_ico_sphere(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_object"
    bl_label = "ICO"
    bl_description = "ICO𐶐܂"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_ico_sphere_add()
        print("ICO𐶐܂B")
        return {'FINISHED'}


class MYADDON_OT_export_scene(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    bl_idname = "myaddon.myaddon_ot_export_scene"
    bl_label = "シーンJSON出力"
    bl_description = "現在のシーンをゲーム用 scene.json へ出力保存します"

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

        scene = context.scene

        valid_path_ids = {
            getattr(o, "enemy_path_id", "None") 
            for o in scene.objects 
            if o.type == 'CURVE' or o.name.startswith("EnemyPath")
        }
        all_enemy_names = {
            o.name for o in scene.objects 
            if getattr(o, "game_obj_type", "NONE") == "ENEMY" or o.name.startswith("AIEnemy")
        }

        for obj in scene.objects:
            if getattr(obj, "game_obj_type", "NONE") == "ENEMY" or obj.name.startswith("AIEnemy"):
                if _is_unset_text(getattr(obj, "enemy_type", None)):
                    obj.enemy_type = "VF3"
                path_id = getattr(obj, "enemy_path_id", "None")
                if path_id != "None" and path_id not in valid_path_ids:
                    obj.enemy_path_id = "None"
                trig = getattr(obj, "enemy_reinforcement_trigger_name", "")
                if trig and trig not in all_enemy_names:
                    obj.enemy_reinforcement_trigger_name = ""

        errors, warnings = validation.validate_and_store(scene)
        if errors:
            first_err = str(errors[0])
            self.report({'ERROR'}, f"エクスポートエラー: {first_err}")
            return {'CANCELLED'}

        self.export()
        if warnings:
            self.report({'WARNING'}, f"警告 {len(warnings)} 件あり: {self.filepath} に保存しました")
            return {'FINISHED'}

        self.report({'INFO'}, f"シーンを正常にエクスポートしました: {self.filepath}")
        return {'FINISHED'}


class MYADDON_OT_validate_scene(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_validate_scene"
    bl_label = "シーンチェック"
    bl_description = "現在のシーンがゲーム用データとして正しいかチェックします"
    bl_options = {'REGISTER'}

    def execute(self, context):
        errors, warnings = validation.validate_and_store(context.scene)
        if errors:
            self.report({'ERROR'}, f"バリデーションエラー: {errors[0]} (全{len(errors)}件)")
        elif warnings:
            self.report({'WARNING'}, f"バリデーション警告 {len(warnings)}件")
        else:
            self.report({'INFO'}, "バリデーションOK: エラーはありません")
        return {'FINISHED'}


class MYADDON_OT_playtest_game(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_playtest_game"
    bl_label = "ゲームをプレイ"
    bl_description class MYADDON_OT_apply_active_properties_to_selection(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_apply_active_properties_to_selection"
    bl_label = "選択オブジェクトへ一括適用"
    bl_description = "アクティブオブジェクトのゲーム用設定を選択中の全オブジェクトへコピーします"
    bl_options = {'REGISTER', 'UNDO'}

    copy_collider: bpy.props.BoolProperty(name="コライダーもコピー", default=True)

    def execute(self, context):
        active = context.active_object
        if not active:
            self.report({'ERROR'}, "コピー元オブジェクトをアクティブにしてください。")
            return {'CANCELLED'}

        targets = [obj for obj in context.selected_objects if obj != active]
        if not targets:
            self.report({'ERROR'}, "コピー先のオブジェクトを一緒に選択してください。")
            return {'CANCELLED'}

        property_names = [
            "game_obj_type",
            "game_model_file",
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
        return {'FINISHED'}��ました: {exc}")
            return {'CANCELLED'}

        if warnings:
            self.report({'WARNING'}, f"警告 {len(warnings)}件あり。ゲームを起動しました。")
        else:
            self.report({'INFO'}, "ゲームを起動しました。")
        return {'FINISHED'}


class MYADDON_OT_apply_active_properties_to_selection(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_apply_active_properties_to_selection"
    bl_label = "選択オブジェクトへ一括適用"
    bl_description = "アクティブオブジェクトのゲーム用設定を選択中の全オブジェクトへコピーします"
    bl_options = {'REGISTER', 'UNDO'}

    copy_collider: bpy.props.BoolProperty(name="コライダーもコピー", default=True)

    def execute(self, context):
        active = context.active_object
        if not active:
            self.report({'ERROR'}, "コピー元オブジェクトをアクティブにしてください。")
            return {'CANCELLED'}:
            self.report({'ERROR'}, f"Q[EXE܂: {exe_path}")
            return {'CANCELLED'}

        working_dir = _default_game_working_dir()
        if not os.path.isdir(working_dir):
            working_dir = os.path.dirname(exe_path)

        try:
            subprocess.Popen([exe_path], cwd=working_dir)
        except Exception as exc:
            self.report({'ERROR'}, f"Q[NɎs܂: {exc}")
            return {'CANCELLED'}

        if warnings:
            self.report({'WARNING'}, f"x {len(warnings)}܂AQ[N܂B")
        else:
            self.report({'INFO'}, "Q[N܂B")
        return {'FINISHED'}


class MYADDON_OT_apply_active_properties_to_selection(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_apply_active_properties_to_selection"
    bl_label = "I𒆂ֈꊇKp"
    bl_description = "ANeBuIuWFNg̃Q[pݒAI𒆂̑IuWFNgփRs[܂"
    bl_options = {'REGISTER', 'UNDO'}

    copy_collider: bpy.props.BoolProperty(name="RC_[Rs[", default=True)

    def execute(self, context):
        active = context.active_object
        if not active:
            self.report({'ERROR'}, "Rs[ɂIuWFNgANeBuɂĂB")
            return {'CANCELLED'}

        targets = [obj for obj in context.selected_objects if obj != active]
        if not targets:
            self.report({'ERROR'}, "Rs[ɂIuWFNgꏏɑIĂB")
            return {'CANCELLED'}

        property_names = [
            "game_obj_type",
            "game_model_file",
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
        self.report({'INFO'}, f"{copied_count}̃IuWFNg֐ݒꊇKp܂B")
        return {'FINISHED'}



def _parse_ai_level_prompt(prompt, latest_prompt=None):
    normalized = str(prompt or "").casefold()
    if latest_prompt is None:
        latest_prompt = prompt
    latest_normalized = str(latest_prompt or "").casefold()
    
    plan = {
        "style": "CITY",
        "shape": "CUBE",
        "density_multiplier": 1.0,
        "is_modifier": False,
        "scale_modifiers": __import__('mathutils').Vector((1.0, 1.0, 1.0)),
        "randomize_location": False,
        "match_player_size": False
    }

    if _prompt_has(latest_normalized, "プレイヤー", "player", "同じ", "同サイズ", "同じサイズ"):
        plan["match_player_size"] = True

    if _prompt_has(latest_normalized, "内", "中", "周り", "ランダム", "配置", "入る", "ばらまく", "散らす", "random", "inside", "around", "place"):
        plan["randomize_location"] = True

    if _prompt_has(latest_normalized, "小さく", "小さい", "縮小", "small", "shrink"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((0.5, 0.5, 0.5))
        plan["is_modifier"] = True
    elif _prompt_has(latest_normalized, "大きく", "大きい", "拡大", "big", "large", "huge"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((2.0, 2.0, 2.0))
        plan["is_modifier"] = True
    elif _prompt_has(latest_normalized, "高く", "高い", "tall", "height"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((1.0, 1.0, 2.0))
        plan["is_modifier"] = True
    elif _prompt_has(latest_normalized, "低く", "低い", "short"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((1.0, 1.0, 0.5))
        plan["is_modifier"] = True
    elif _prompt_has(latest_normalized, "長く", "長い", "long"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((2.0, 1.0, 1.0))
        plan["is_modifier"] = True
    elif _prompt_has(latest_normalized, "平たく", "平ら", "flat"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((1.0, 1.0, 0.2))
        plan["is_modifier"] = True
        
    if plan["match_player_size"]:
        plan["is_modifier"] = False

    if check( "迷路", "maze"):
        plan["style"] = "MAZE"
    elif check( "防衛線", "防壁", "ライン", "defense", "line"):
        plan["style"] = "DEFENSE_LINE"
    elif check( "アリーナ", "広場", "arena", "open"):
        plan["style"] = "ARENA"
    elif check( "市街地", "街", "ビル", "city", "building"):
        plan["style"] = "CITY"

    if check( "円柱", "柱", "cylinder", "pillar"):
        plan["shape"] = "CYLINDER"

    if check( "たくさん", "大量", "多い", "many", "dense", "2倍", "２倍"):
        plan["density_multiplier"] = 2.0
    elif check( "少し", "少ない", "まばら", "few", "sparse", "半分"):
        plan["density_multiplier"] = 0.5

    return plan

def _delete_ai_generated_obstacles(scene):
    import bpy
    to_delete = []
    
    col_name = "AI_Obstacles"
    obs_col = bpy.data.collections.get(col_name)
    
    for obj in scene.objects:
        if obj.get("ai_generated_obstacle"):
            to_delete.append(obj)
        elif obs_col and obj.name in obs_col.objects:
            to_delete.append(obj)
            
    for obj in set(to_delete):
        try:
            bpy.data.objects.remove(obj, do_unlink=True)
        except Exception:
            pass

def _build_ai_level_obstacles(context, obstacles_to_create, shape_type, prompt=""):
    import bpy
    scene = context.scene
    created_objs = []
    
    # 境界ボックスの取得
    from mathutils import Vector
    center, extents = _get_target_bounds(scene, prompt)
    
    col_name = "AI_Obstacles"
    obs_col = bpy.data.collections.get(col_name)
    if not obs_col:
        obs_col = bpy.data.collections.new(col_name)
        scene.collection.children.link(obs_col)
        
    for obs_data in obstacles_to_create:
        loc = obs_data["loc"]
        scale = obs_data["scale"]
        
        loc.z = center.z - extents.z + scale.z
        
        # 1. 境界内チェック (Clamp to StageBounds)
        loc = _clamp_point_to_box(loc, center, extents, Vector((scale.x, scale.y, 0.0)))
        
        # 2. 衝突回避 (他障害物と被らないように押し出し)
        loc = _push_out_of_obstacles(loc, scene)
        
        if shape_type == "CYLINDER":
            bpy.ops.mesh.primitive_cylinder_add(location=loc)
        else:
            bpy.ops.mesh.primitive_cube_add(location=loc)
            
        obj = context.active_object
        obj.name = f"LevelObstacle_{1000 + len(created_objs)}"
        obj.scale = scale
        obj["ai_generated_obstacle"] = True
        obj["game_obj_type"] = "OBSTACLE"
        
        for c in obj.users_collection:
            c.objects.unlink(obj)
        obs_col.objects.link(obj)
        
        created_objs.append(obj)
        
    return created_objs

class MYADDON_OT_ai_chat_clear(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_chat_clear"
    bl_label = "会話履歴をクリア"
    bl_description = "障害物ジェネレーターの対話履歴をリセットします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if hasattr(context.scene, "myaddon_ai_chat_history"):
            context.scene.myaddon_ai_chat_history.clear()
        self.report({'INFO'}, "会話履歴をクリアしました")
        return {'FINISHED'}

class MYADDON_OT_ai_chat_revert(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_chat_revert"
    bl_label = "プロンプトを再利用"
    bl_description = "クリックした履歴のプロンプトを入力欄にコピーします"
    bl_options = {'REGISTER', 'UNDO'}

    target_index: bpy.props.IntProperty(name="Target Index", default=-1)

    def execute(self, context):
        scene = context.scene
        if not hasattr(scene, "myaddon_ai_chat_history") or self.target_index < 0:
            return {'CANCELLED'}
        
        history = scene.myaddon_ai_chat_history
        if self.target_index >= len(history):
            return {'CANCELLED'}
            
        target_msg = history[self.target_index]
        if target_msg.role != "USER":
            return {'CANCELLED'}
            
        scene.myaddon_ai_level_prompt = target_msg.content
        
        self.report({'INFO'}, "過去のプロンプトを入力欄にコピーしました")
        return {'FINISHED'}

class MYADDON_OT_ai_generate_level_obstacles(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_generate_level_obstacles"
    bl_label = "AIで障害物を自動配置"
    bl_description = "コンセプトに合わせてフィールド上に障害物を自動生成・配置します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        import bpy
        import random
        from mathutils import Vector
        import math
        scene = context.scene
        
        prompt = getattr(scene, "myaddon_ai_level_prompt", "")
        seed = int(getattr(scene, "myaddon_ai_level_seed", 1))
        clear_existing = getattr(scene, "myaddon_ai_level_clear_existing", True)
        
        if clear_existing:
            _delete_ai_generated_obstacles(scene)
            
        if prompt.strip():
            msg = scene.myaddon_ai_chat_history.add()
            msg.role = "USER"
            msg.content = prompt.strip()
            msg.seed = seed
            scene.myaddon_ai_level_prompt = ""
            
        history = list(scene.myaddon_ai_chat_history)
        
        full_text = " ".join([m.content for m in history]) if history else ""
        latest_text = history[-1].content if history else ""
        plan = _parse_ai_level_prompt(full_text, latest_prompt=latest_text)
        
        force_params_json = getattr(scene, "myaddon_ai_level_force_params", "")
        if force_params_json:
            try:
                import json
                forced = json.loads(force_params_json)
                plan.update(forced)
            except Exception as e:
                print("Failed to parse level force params:", e)
            scene.myaddon_ai_level_force_params = ""
            
        rng = random.Random(seed)
        
        center, extents = _get_target_bounds(scene, prompt)
        
        style = plan["style"]
        shape = plan["shape"]
        density = plan["density_multiplier"]
        target_count = getattr(scene, "myaddon_ai_level_count", 4)
        base_count = max(1, int(target_count * density))
        
        obstacles_to_create = []
        
        learning = _load_ai_learning()
        weights = learning["level_style_weights"].get(style, {})
        w_sx = weights.get("scale_x", 1.0)
        w_sy = weights.get("scale_y", 1.0)
        w_sz = weights.get("scale_z", 1.0)
        
        if style == "CITY":
            cols = max(1, int(math.sqrt(base_count)))
            rows = max(1, int(math.ceil(base_count / cols)))
            spacing_x = (extents.x * 2 * 0.8) / cols if cols > 1 else 0
            spacing_y = (extents.y * 2 * 0.8) / rows if rows > 1 else 0
            
            grid_points = [(r, c) for r in range(rows) for c in range(cols)]
            rng.shuffle(grid_points)
            
            for i in range(min(base_count, len(grid_points))):
                r, c = grid_points[i]
                lx = center.x - extents.x*0.8 + c * spacing_x + rng.uniform(-2, 2)
                ly = center.y - extents.y*0.8 + r * spacing_y + rng.uniform(-2, 2)
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((rng.uniform(1.0, 3.0) * w_sx, rng.uniform(1.0, 3.0) * w_sy, rng.uniform(4.0, 12.0) * w_sz))
                })
        elif style == "DEFENSE_LINE":
            spacing = (extents.x * 2 * 0.9) / max(1, base_count)
            for i in range(base_count):
                lx = center.x - extents.x*0.9 + i * spacing
                ly = center.y + extents.y * rng.uniform(0.1, 0.3)
                obstacles_to_create.append({
                    "loc": Vector((lx, ly + rng.uniform(-1, 1), 0)),
                    "scale": Vector((rng.uniform(2.0, 6.0) * w_sx, rng.uniform(1.0, 2.0) * w_sy, rng.uniform(2.0, 4.0) * w_sz))
                })
        elif style == "MAZE":
            for i in range(base_count):
                lx = center.x + rng.uniform(-extents.x*0.8, extents.x*0.8)
                ly = center.y + rng.uniform(-extents.y*0.8, extents.y*0.8)
                is_horizontal = rng.random() > 0.5
                sx = rng.uniform(6.0, 15.0) if is_horizontal else rng.uniform(1.0, 2.0)
                sy = rng.uniform(1.0, 2.0) if is_horizontal else rng.uniform(6.0, 15.0)
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((sx * w_sx, sy * w_sy, rng.uniform(3.0, 6.0) * w_sz))
                })
        elif style == "ARENA":
            for i in range(base_count):
                angle = (i / base_count) * 2 * math.pi
                r = min(extents.x, extents.y) * 0.7
                lx = center.x + math.cos(angle) * r
                ly = center.y + math.sin(angle) * r
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((2.0 * w_sx, 2.0 * w_sy, rng.uniform(3.0, 8.0) * w_sz))
                })
                
        if plan.get("match_player_size"):
            player_obj = next((o for o in scene.objects if o.get("game_obj_type") == 'PLAYER' or "Player" in o.name), None)
            if player_obj:
                psx = max(0.5, abs(player_obj.dimensions.x) * 0.5)
                psy = max(0.5, abs(player_obj.dimensions.y) * 0.5)
                psz = max(0.5, abs(player_obj.dimensions.z) * 0.5)
                player_scale = Vector((psx, psy, psz))
            else:
                player_scale = Vector((1.0, 1.0, 2.0))
            for obs in obstacles_to_create:
                obs["scale"] = player_scale
                
        created_objs = _build_ai_level_obstacles(context, obstacles_to_create, shape, prompt)
        
        msg_ai = scene.myaddon_ai_chat_history.add()
        msg_ai.role = "AI"
        msg_ai.content = f"{len(created_objs)}個の障害物を生成しました (スタイル: {plan['style']}, 形状: {plan['shape']})"
        msg_ai.gemini_ratio = 100 if force_params_json else 0
        
        self.report({'INFO'}, f"{len(created_objs)}個のAI障害物を配置しました")
        
        return {'FINISHED'}

class MYADDON_OT_ai_enemy_chat_clear(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_enemy_chat_clear"
    bl_label = "敵会話履歴をクリア"
    bl_description = "敵ジェネレーターの対話履歴をリセットします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if hasattr(context.scene, "myaddon_ai_enemy_chat_history"):
            context.scene.myaddon_ai_enemy_chat_history.clear()
        self.report({'INFO'}, "敵生成の会話履歴をクリアしました")
        return {'FINISHED'}

class MYADDON_OT_ai_enemy_chat_revert(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_enemy_chat_revert"
    bl_label = "この時点にロールバック"
    bl_description = "クリックした質問の直後までモデルの状態を戻します（履歴は保持されます）"
    bl_options = {'REGISTER', 'UNDO'}

    target_index: bpy.props.IntProperty(name="Target Index", default=-1)

    def execute(self, context):
        scene = context.scene
        if not hasattr(scene, "myaddon_ai_enemy_chat_history") or self.target_index < 0:
            return {'CANCELLED'}
        
        history = scene.myaddon_ai_enemy_chat_history
        if self.target_index >= len(history):
            return {'CANCELLED'}
            
        target_msg = history[self.target_index]
        if target_msg.role != "USER":
            return {'CANCELLED'}
            
        ai_msg_index = self.target_index + 1
        if ai_msg_index < len(history) and history[ai_msg_index].role == "AI":
            snapshot_data = history[ai_msg_index].snapshot
            if snapshot_data:
                _restore_ai_enemy_snapshot(scene, snapshot_data)
                
                prompt = target_msg.content
                if prompt.startswith("(個別編集) "):
                    prompt = prompt.replace("(個別編集) ", "", 1)
                scene.myaddon_ai_enemy_prompt = prompt
                
                self.report({'INFO'}, "指定した質問の直後まで状態をロールバックしました（履歴は保持）")
                return {'FINISHED'}
                
        self.report({'WARNING'}, "スナップショットが見つからないためロールバックできません")
        return {'CANCELLED'}

class MYADDON_OT_ai_generate_enemy_plan(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_generate_enemy_plan"
    bl_label = "AI敵生成"
    bl_description = "指定位置に敵を配置します"
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

        history = list(getattr(scene, "myaddon_ai_enemy_chat_history", []))
        prompt = getattr(scene, "myaddon_ai_enemy_prompt", "").strip()
        count = max(1, int(getattr(scene, "myaddon_ai_enemy_count", 6)))
        seed = max(0, int(getattr(scene, "myaddon_ai_enemy_seed", 1)))

        if prompt:
            msg = scene.myaddon_ai_enemy_chat_history.add()
            msg.role = "USER"
            msg.content = prompt
            msg.seed = seed
            history.append(msg)
            scene.myaddon_ai_enemy_prompt = ""

        full_text = " ".join([msg.content for msg in history]).casefold()
        style, motion = _parse_ai_enemy_prompt(full_text)
        wave_delay = max(0, int(getattr(scene, "myaddon_ai_enemy_wave_delay", 90)))
        rng = random.Random(seed)
        force_params_json = getattr(scene, "myaddon_ai_enemy_force_params", "")
        if force_params_json:
            try:
                import json
                forced = json.loads(force_params_json)
                style = forced.get("style", style)
                motion.update(forced.get("motion", {}))
            except Exception as e:
                print("Failed to parse force_params:", e)
            scene.myaddon_ai_enemy_force_params = ""

        latest_msg = history[-1].content if history else ""

        learning = _load_ai_learning()
        weights = learning["enemy_style_weights"].get(style, {})
        w_speed = weights.get("speed", 1.0)

        lane_count = max(1, (count + 1) // 2)
        formation_offsets = _build_ai_formation_offsets(count, extents, motion)
        speed_by_style = {
            'BALANCED': 0.050,
            'AMBUSH': 0.060,
            'SWARM': 0.072,
            'PATROL': 0.038,
        }
        opener_count = min(count, 4 if style == 'SWARM' else 2)
        if motion.get("opener") == "ALL":
            opener_count = count
        elif motion.get("opener") == "ONE":
            opener_count = 1
        margin = Vector((0.5, 0.5, 0.5))
        if getattr(scene, "myaddon_ai_enemy_clear_existing", True):
            _delete_ai_generated_objects(scene)

        builtin_plan = {"enemies": []}

        for index in range(count):
            pair_index = index // 2
            lane_t = 0.0 if lane_count <= 1 else pair_index / (lane_count - 1)
            side = -1.0 if index % 2 == 0 else 1.0
            z_span = min(extents.z * 0.32, 4.0)
            height = rng.uniform(-z_span, z_span)

            if style == 'SCATTER':
                cols = max(1, math.ceil(math.sqrt(count)))
                rows = max(1, math.ceil(count / cols))
                cell_w = (extents.x * 2.0 * 0.78) / max(1, cols)
                cell_h = (extents.y * 2.0 * 0.78) / max(1, rows)

                c = index % cols
                r = index // cols

                cell_center_x = (center.x - extents.x * 0.78) + (c + 0.5) * cell_w
                cell_center_y = (center.y - extents.y * 0.78) + (r + 0.5) * cell_h

                jx = rng.uniform(-cell_w * 0.35, cell_w * 0.35)
                jy = rng.uniform(-cell_h * 0.35, cell_h * 0.35)

                spawn = Vector((cell_center_x + jx, cell_center_y + jy, center.z + height))
            else:
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

            def snap_to_terrain(point, terrain_objs):
                return _snap_point_to_terrain(point, terrain_objs)

            base_type = getattr(scene, "myaddon_ai_enemy_base_type", "VF3").strip()
            if not base_type:
                base_type = "VF3"
            enemy_type = base_type
            if motion and motion.get("enemy_type"):
                enemy_type = motion.get("enemy_type")

            terrain_objs = [
                obj for obj in scene.objects 
                if obj.type == 'MESH' 
                and obj.name != "StageBounds" 
                and not obj.name.startswith("AIEnemy") 
                and not obj.name.startswith("Enemy")
                and not obj.name.startswith("Camera")
                and not obj.name.startswith("Light")
            ]

            clean_type = enemy_type.upper().replace("-", "").replace("_", "").replace(" ", "")
            is_ground_type = ("VF3" in clean_type or "GROUND" in clean_type or "TANK" in clean_type or "LAND" in clean_type)

            speed_jitter = 1.0 if motion.get("keep_formation") else rng.uniform(0.88, 1.12)
            speed = speed_by_style.get(style, 0.050) * motion.get("speed_multiplier", 1.0) * speed_jitter * w_speed
            loop = motion["loop"] if motion.get("loop") is not None else style == 'PATROL'
            points = _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, pair_index, rng, formation_offset)

            if is_ground_type:
                # 1. 水平座標 X, Y をステージ枠(StageBounds)内に保持
                spawn = _clamp_point_to_box(spawn, center, extents, margin)

                # 2. 地形標高(snap_to_terrain)へ確実スナップ
                spawn_x, spawn_y, spawn_z = snap_to_terrain(spawn, terrain_objs)

                # 3. スナップ後も X, Y 座標が枠外に出ないよう再クランプし、標高 Z は山肌位置に固定
                clamped_xy = _clamp_point_to_box(Vector((spawn_x, spawn_y, 0.0)), center, extents, margin)
                spawn = Vector((clamped_xy.x, clamped_xy.y, spawn_z))

                # 4. 地上敵(VF3)は空中の飛行パス(EnemyPath)を生成せず、山肌表面に固定
                points = [spawn]
                motion["enemy_path_id"] = None
            elif motion.get("pattern") == "EDGE_ORBIT" and points:
                spawn = points[0]
            handle_type = 'VECTOR' if motion.get("pattern") == "EDGE_ORBIT" and motion.get("respect_bounds") else 'AUTO'

            trigger_index = -1
            delay_frames = wave_delay
            if index >= opener_count:
                trigger_index = index - opener_count
                delay_jitter = wave_delay // 5
                delay_frames = max(0, wave_delay + rng.randint(-delay_jitter, delay_jitter))
                
            enemy_plan = {
                "spawn": {"x": spawn.x, "y": spawn.y, "z": spawn.z},
                "path": [{"x": p.x, "y": p.y, "z": p.z} for p in points],
                "enemy_type": enemy_type,
                "loop": loop,
                "speed": speed,
                "trigger_index": trigger_index,
                "delay_frames": delay_frames,
                "handle_type": handle_type,
            }
            if not is_ground_type and motion.get("enemy_path_id"):
                enemy_plan["enemy_path_id"] = motion["enemy_path_id"]
                
            builtin_plan["enemies"].append(enemy_plan)

        ollama_used = False
        blueprints = _sanitize_enemy_plan_data(builtin_plan, count, center, extents, player)

        if getattr(scene, "myaddon_ai_enemy_clear_existing", True):
            _delete_ai_generated_objects(scene)

        generated_objects, generated_enemies = _create_ai_enemy_objects_from_blueprints(
            scene,
            collection,
            blueprints,
            full_text,
            player,
            "OLLAMA" if ollama_used else "BUILTIN",
        )

        for o in scene.objects:
            o.select_set(False)
        for obj in generated_objects:
            obj.select_set(True)
        if generated_enemies:
            context.view_layer.objects.active = generated_enemies[0]

        errors, warnings = validation.validate_and_store(scene)
        _auto_export_scene_json()
        
        msg_ai = scene.myaddon_ai_enemy_chat_history.add()
        msg_ai.role = "AI"
        msg_ai.snapshot = _save_ai_enemy_snapshot(scene)
        
        if ollama_used:
            msg_ai.content = f"{len(generated_enemies)}体の敵を生成しました (Ollama最適化済)"
            msg_ai.gemini_ratio = 100
        else:
            msg_ai.content = f"{len(generated_enemies)}体の敵を配置しました (内蔵AI)"
            msg_ai.gemini_ratio = 0

        if errors:
            self.report({'WARNING'}, f"生成完了（エラー{len(errors)}件）")
        elif warnings:
            self.report({'WARNING'}, f"生成完了（警告{len(warnings)}件）")
        else:
            self.report({'INFO'}, f"生成完了: 敵{len(generated_enemies)}体")
        return {'FINISHED'}


class MYADDON_OT_create_enemy_path(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_enemy_path"
    bl_label = "GspXzu"
    bl_description = "G@Ǐ]XvCpX܂"
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

        for o in scene.objects:
            o.select_set(False)
        obj.select_set(True)
        context.view_layer.objects.active = obj

        if active_enemy:
            active_enemy.enemy_path_id = path_id

        print(f"GspXzu܂: {path_id}")
        return {'FINISHED'}


class MYADDON_OT_assign_selected_enemy_path(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_assign_selected_enemy_path"
    bl_label = "IpXGɊ蓖"
    bl_description = "I𒆂̓GX|[n_ցAIĂpX蓖Ă܂"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        active = context.active_object
        if not active or getattr(active, "game_obj_type", "NONE") != "ENEMY":
            self.report({'ERROR'}, "蓖Đ̓GX|[n_ANeBuɂĂB")
            return {'CANCELLED'}

        selected_paths = [
            obj for obj in context.selected_objects
            if obj != active and _is_enemy_path_object(obj)
        ]

        if not selected_paths:
            self.report({'ERROR'}, "蓖Ă EnemyPath J[uꏏɑIĂB")
            return {'CANCELLED'}

        active.enemy_path_id = _get_enemy_path_id(selected_paths[0])
        self.report({'INFO'}, f"{active.name}  {active.enemy_path_id} 蓖Ă܂B")
        return {'FINISHED'}


class MYADDON_OT_assign_selected_reinforcement_trigger(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_assign_selected_reinforcement_trigger"
    bl_label = "IG𑝉gK[ɐݒ"
    bl_description = "ANeBuȓGX|[n_AIʂ̓G|ꂽɏo܂"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        active = context.active_object
        if not active or getattr(active, "game_obj_type", "NONE") != "ENEMY":
            self.report({'ERROR'}, "ƂďoGX|[n_ANeBuɂĂB")
            return {'CANCELLED'}

        selected_triggers = [
            obj for obj in context.selected_objects
            if obj != active and getattr(obj, "game_obj_type", "NONE") == "ENEMY"
        ]

        if not selected_triggers:
            self.report({'ERROR'}, "トリガーにする敵オブジェクトを選択してください。")
            return {'CANCELLED'}

        active.enemy_reinforcement_trigger_name = ", ".join([obj.name for obj in selected_triggers])
        if getattr(active, "enemy_reinforcement_delay_frames", 0) < 0:
            active.enemy_reinforcement_delay_frames = 0

        validation.validate_and_store(context.scene)
        self.report({'INFO'}, f"[{active.enemy_reinforcement_trigger_name}] が倒されたら {active.name} が出現します。")
        return {'FINISHED'}


class MYADDON_OT_assign_ai_reinforcement_trigger(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_assign_ai_reinforcement_trigger"
    bl_label = "選択中の敵をAIの増援トリガーに設定"
    bl_description = "現在選択されている敵オブジェクトをAI増援のトリガーとして設定します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        selected_triggers = [
            obj for obj in context.selected_objects
            if getattr(obj, "game_obj_type", "NONE") == "ENEMY"
        ]

        if not selected_triggers:
            self.report({'ERROR'}, "トリガーにする敵オブジェクトを選択してください。")
            return {'CANCELLED'}

        names = ", ".join([obj.name for obj in selected_triggers])
        context.scene.myaddon_ai_enemy_trigger_target = names

        self.report({'INFO'}, f"AI増援トリガーに設定しました: {names}")
        return {'FINISHED'}


class MYADDON_OT_create_stage_bounds(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_stage_bounds"
    bl_label = "Xe[W͈͂zu"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_cube_add(size=2.0)
        obj = context.active_object
        obj.name = "StageBounds"
        obj.scale = (10.0, 10.0, 10.0)
        obj.game_obj_type = 'OBSTACLE'
        obj.display_type = 'WIRE'
        print("C[t[̃Xe[W͈͂zu܂B")
        return {'FINISHED'}


class MYADDON_OT_create_spawn_point(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_spawn_point"
    bl_label = "X|[zu"
    bl_options = {'REGISTER', 'UNDO'}

    spawn_type: bpy.props.StringProperty(default="PLAYER")

    def execute(self, context):
        bpy.ops.object.empty_add(type='SINGLE_ARROW', radius=2.0)
        obj = context.active_object

        if self.spawn_type == "PLAYER":
            obj.name = "PlayerSpawn"
            obj.game_obj_type = 'PLAYER'
            print("@̃X|[n_zu܂B")
        else:
            obj.name = "EnemyRespawn"
            obj.game_obj_type = 'ENEMY'
            print("G̃X|[n_zu܂B")

        return {'FINISHED'}


class MYADDON_OT_randomize_ai_level_seed(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_randomize_ai_level_seed"
    bl_label = "レベルシード値をランダム化"
    bl_description = "シード値をランダムな数値に設定します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        import random
        context.scene.myaddon_ai_level_seed = random.randint(1, 999999)
        return {'FINISHED'}

class MYADDON_OT_ai_edit_selected_obstacle(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_edit_selected_obstacle"
    bl_label = "選択した障害物だけを再生成"
    bl_description = "選択中の障害物を、個別の指示で再生成します"
    bl_options = {'REGISTER', 'UNDO'}

    level_prompt: bpy.props.StringProperty(
        name="専用プロンプト",
        description="この障害物専用の形状指示",
        default="",
    )
    seed: bpy.props.IntProperty(
        name="専用シード",
        description="この障害物専用のシード値",
        default=1,
    )

    def invoke(self, context, event):
        scene = context.scene
        self.level_prompt = getattr(scene, "myaddon_ai_level_prompt", "")
        self.seed = getattr(scene, "myaddon_ai_level_seed", 1)
        return context.window_manager.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "level_prompt")
        layout.prop(self, "seed")
        layout.label(text="現在位置を基準に新しい障害物を生成します", icon='INFO')

    def execute(self, context):
        import random
        import bpy
        from mathutils import Vector
        scene = context.scene
        
        selected_obstacles = [obj for obj in context.selected_objects if obj.get("game_obj_type", "") == 'OBSTACLE' or obj.name.startswith("LevelObstacle")]
        
        if not selected_obstacles:
            self.report({'ERROR'}, "再生成したい障害物を選択してください")
            return {'CANCELLED'}

        class DummyMsg:
            def __init__(self, role, content):
                self.role = role
                self.content = content
        
        history = [DummyMsg(m.role, m.content) for m in scene.myaddon_ai_chat_history]
        if self.level_prompt.strip():
            history.append(DummyMsg("USER", self.level_prompt.strip()))
            
        full_text = " ".join([m.content for m in history]) if history else ""
        latest_text = history[-1].content if history else ""
        
        plan = _parse_ai_level_prompt(full_text, latest_prompt=latest_text)
        
        gemini_plan = None
        if len(history) > 0:
            gemini_plan = _request_ollama_level_plan(context, scene, history)
            if gemini_plan:
                plan["style"] = gemini_plan.get("style", plan["style"])
                plan["shape"] = gemini_plan.get("shape", plan["shape"])
                plan["density_multiplier"] = gemini_plan.get("density_multiplier", plan["density_multiplier"])
                if "match_player_size" in gemini_plan and not plan["match_player_size"]:
                    plan["match_player_size"] = gemini_plan["match_player_size"]
                if "randomize_location" in gemini_plan:
                    plan["randomize_location"] = gemini_plan["randomize_location"]
            
        center, extents = _get_target_bounds(scene, full_text)
        rng = random.Random(self.seed)
        shape = plan["shape"]
        
        col_name = "AI_Obstacles"
        obs_col = bpy.data.collections.get(col_name)
        if not obs_col:
            obs_col = bpy.data.collections.new(col_name)
            scene.collection.children.link(obs_col)

        updated_count = 0
        base_plan = _parse_ai_level_prompt(full_text)
        base_style = base_plan["style"]
        
        for obs_obj in selected_obstacles:
            if plan.get("is_modifier"):
                obs_obj.scale.x *= plan["scale_modifiers"].x
                obs_obj.scale.y *= plan["scale_modifiers"].y
                obs_obj.scale.z *= plan["scale_modifiers"].z
                
                # Update learning dictionary
                learning = _load_ai_learning()
                if base_style not in learning["level_style_weights"]:
                    learning["level_style_weights"][base_style] = {}
                weights = learning["level_style_weights"][base_style]
                
                if plan["scale_modifiers"].x > 1.0: weights["scale_x"] = weights.get("scale_x", 1.0) * 1.1
                elif plan["scale_modifiers"].x < 1.0: weights["scale_x"] = weights.get("scale_x", 1.0) * 0.9
                
                if plan["scale_modifiers"].y > 1.0: weights["scale_y"] = weights.get("scale_y", 1.0) * 1.1
                elif plan["scale_modifiers"].y < 1.0: weights["scale_y"] = weights.get("scale_y", 1.0) * 0.9
                
                if plan["scale_modifiers"].z > 1.0: weights["scale_z"] = weights.get("scale_z", 1.0) * 1.1
                elif plan["scale_modifiers"].z < 1.0: weights["scale_z"] = weights.get("scale_z", 1.0) * 0.9
                
                _save_ai_learning(learning)
                
                # Enforce bounds
                margin = Vector((obs_obj.scale.x, obs_obj.scale.y, 0.0))
                obs_obj.location = _clamp_point_to_box(obs_obj.location, center, extents, margin)
                if obs_obj.location.z < obs_obj.scale.z:
                    obs_obj.location.z = obs_obj.scale.z
                
                obs_obj["myaddon_ai_level_prompt"] = self.level_prompt
                updated_count += 1
                continue
                
            spawn_loc = obs_obj.location.copy()
            
            # Remove old obstacle
            bpy.data.objects.remove(obs_obj, do_unlink=True)
            
            if plan.get("match_player_size"):
                player_obj = next((o for o in scene.objects if o.get("game_obj_type") == 'PLAYER' or "Player" in o.name), None)
                if player_obj:
                    sx = max(0.5, abs(player_obj.dimensions.x) * 0.5)
                    sy = max(0.5, abs(player_obj.dimensions.y) * 0.5)
                    sz = max(0.5, abs(player_obj.dimensions.z) * 0.5)
                else:
                    sx, sy, sz = 1.0, 1.0, 2.0
            else:
                sx = rng.uniform(1.0, 3.0)
                sy = rng.uniform(1.0, 3.0)
                sz = rng.uniform(4.0, 12.0)
                
                if plan["style"] == "MAZE":
                    is_horizontal = rng.random() > 0.5
                    sx = rng.uniform(6.0, 15.0) if is_horizontal else rng.uniform(1.0, 2.0)
                    sy = rng.uniform(1.0, 2.0) if is_horizontal else rng.uniform(6.0, 15.0)
                elif plan["style"] == "DEFENSE_LINE":
                    sx = rng.uniform(2.0, 6.0)
                    sy = rng.uniform(1.0, 2.0)
                    sz = rng.uniform(2.0, 4.0)

            scale = Vector((sx, sy, sz))
            
            if plan.get("randomize_location"):
                spawn_loc.x = center.x + rng.uniform(-extents.x*0.8, extents.x*0.8)
                spawn_loc.y = center.y + rng.uniform(-extents.y*0.8, extents.y*0.8)
                spawn_loc.z = center.z - extents.z + scale.z
            else:
                # Random jitter
                spawn_loc.x += rng.uniform(-1.0, 1.0)
                spawn_loc.y += rng.uniform(-1.0, 1.0)
            
            # Enforce bounds
            margin = Vector((scale.x, scale.y, 0.0))
            spawn_loc = _clamp_point_to_box(spawn_loc, center, extents, margin)
            
            # Generate new object
            if shape == "CYLINDER":
                bpy.ops.mesh.primitive_cylinder_add(location=spawn_loc)
            else:
                bpy.ops.mesh.primitive_cube_add(location=spawn_loc)
                
            new_obj = context.active_object
            new_obj.scale = scale
            

            
            new_obj.name = f"LevelObstacle_Regen_{rng.randint(1000,9999)}"
            new_obj["game_obj_type"] = 'OBSTACLE'
            new_obj["myaddon_ai_level_prompt"] = self.level_prompt
            
            if new_obj.name not in obs_col.objects:
                obs_col.objects.link(new_obj)
            if new_obj.name in scene.collection.objects:
                scene.collection.objects.unlink(new_obj)
                
            updated_count += 1
            
        if self.level_prompt.strip():
            msg_user = scene.myaddon_ai_chat_history.add()
            msg_user.role = "USER"
            msg_user.content = self.level_prompt.strip()
            msg_user.seed = self.seed
            
        msg_ai = scene.myaddon_ai_chat_history.add()
        msg_ai.role = "AI"
        msg_ai.content = f"選択された{updated_count}個の障害物を再生成しました (スタイル: {plan['style']}, 形状: {plan['shape']})"
        msg_ai.gemini_ratio = 100 if gemini_plan else 0
        
        self.report({'INFO'}, f"{updated_count}個の障害物を個別に再生成しました")
        return {'FINISHED'}


class MYADDON_OT_randomize_ai_seed(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_randomize_ai_seed"
    bl_label = "シード値をランダム化"
    bl_description = "シード値をランダムな数値に設定します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        import random
        context.scene.myaddon_ai_enemy_seed = random.randint(1, 999999)
        return {'FINISHED'}


class MYADDON_OT_ai_edit_selected_enemy_path(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_edit_selected_enemy_path"
    bl_label = "選択パスの個別再生成"
    bl_description = "選択中のパスだけを個別の設定で再生成します"
    bl_options = {'REGISTER', 'UNDO'}

    motion_prompt: bpy.props.StringProperty(
        name="専用プロンプト",
        description="このパス専用の動きの指示",
        default="",
    )
    seed: bpy.props.IntProperty(
        name="専用シード",
        description="このパス専用のシード値",
        default=1,
    )

    def invoke(self, context, event):
        scene = context.scene
        self.motion_prompt = getattr(scene, "myaddon_ai_enemy_prompt", "")
        self.seed = getattr(scene, "myaddon_ai_enemy_seed", 1)
        return context.window_manager.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "motion_prompt")
        layout.prop(self, "seed")
        layout.label(text="※複数選択時は各パスに対して1回ずつAIが生成を行います", icon='INFO')
        layout.label(text="(シード値はパスごとに自動でずらして重なりを防ぎます)", icon='INFO')

    def execute(self, context):
        scene = context.scene
        selected_paths = [obj for obj in context.selected_objects if _is_enemy_path_object(obj)]
        if not selected_paths:
            self.report({'ERROR'}, "再生成したいEnemyPathを選択してください")
            return {'CANCELLED'}

        center, extents = _stage_bounds_box(scene)
        player = _find_player_spawn_position(scene, center, extents)
        
        history = list(getattr(scene, "myaddon_ai_enemy_chat_history", []))
        full_text = " ".join([m.content for m in history]).casefold() if history else ""
        
        base_style, _ = _parse_ai_enemy_prompt(full_text)
        style, motion = _parse_ai_enemy_prompt(self.motion_prompt)
        # Use base_style if the edit prompt didn't specify a new style
        if style == 'BALANCED' and not _prompt_has(self.motion_prompt, "巡回", "パトロール", "patrol", "挟み撃ち", "背後", "回り込", "ambush", "群れ", "集団", "一団", "大量", "swarm"):
            style = base_style

        # Learning
        learning = _load_ai_learning()
        if base_style not in learning["enemy_style_weights"]:
            learning["enemy_style_weights"][base_style] = {}
        e_weights = learning["enemy_style_weights"][base_style]
        
        speed_mult = motion.get("speed_multiplier", 1.0)
        if speed_mult > 1.1:
            e_weights["speed"] = e_weights.get("speed", 1.0) * 1.1
        elif speed_mult < 0.9:
            e_weights["speed"] = e_weights.get("speed", 1.0) * 0.9
        _save_ai_learning(learning)
            
        rng = random.Random(self.seed)
        wave_delay = max(0, int(getattr(scene, "myaddon_ai_enemy_wave_delay", 90)))
        
        ollama_used = False
        updated_count = 0
        path_metadata = []
        baseline_plans = []
        
        for path_obj in selected_paths:
            path_id = getattr(path_obj, "enemy_path_id", path_obj.name)
            spawn = None
            side = 1.0
            spawn_obj = None
            for obj in scene.objects:
                if getattr(obj, "game_obj_type", "NONE") == "ENEMY" and getattr(obj, "enemy_path_id", "") == path_id:
                    spawn = obj.location.copy()
                    spawn_obj = obj
                    if spawn.x < player.x: side = -1.0
                    break
            
            if not spawn:
                spawn = path_obj.location.copy()

            loop_val = False
            if path_obj.type == 'CURVE' and path_obj.data.splines:
                loop_val = path_obj.data.splines[0].use_cyclic_u
                
            baseline_plan = {
                "spawn": [spawn.x, spawn.y, spawn.z],
                "loop": loop_val,
                "speed": getattr(path_obj, "enemy_path_speed", 0.05),
                "enemy_type": getattr(path_obj, "enemy_type", "VF1-1"),
                "trigger_index": -1,
                "delay_frames": 0,
                "handle_type": "AUTO"
            }
            baseline_plans.append(baseline_plan)
            path_metadata.append({
                "obj": path_obj,
                "spawn": spawn,
                "side": side,
                "spawn_obj": spawn_obj
            })
            
        blueprints = []
        leader_blueprint = None
        if _is_prompt_complex(self.motion_prompt) and path_metadata:
            try:
                class DummyMsg:
                    def __init__(self, content):
                        self.content = content
                        self.role = "USER"
                        self.seed = 1
                
                spawns_str = ", ".join([f"{{'x':{p['spawn'].x}, 'y':{p['spawn'].y}, 'z':{p['spawn'].z}}}" for p in path_metadata])
                msg_content = f"IMPORTANT: Partial regeneration. Enemy count must be exactly {len(path_metadata)}. Current spawn points are: [{spawns_str}]. Please modify the provided Baseline Plan array according to this request: {self.motion_prompt}. Ensure consistency across all enemies if they form a group."
                temp_history = [DummyMsg(msg_content)]
                
                plan_data = _request_ollama_enemy_plan(
                    context,
                    scene,
                    len(path_metadata),
                    self.seed,
                    temp_history,
                    wave_delay,
                    center,
                    extents,
                    player,
                    baseline_plan=baseline_plans
                )
                blueprints = _sanitize_enemy_plan_data(plan_data, len(path_metadata), center, extents, player)
                if blueprints:
                    leader_blueprint = blueprints[0]
                    ollama_used = True
                    
                    shape_override = leader_blueprint.get("shape_override")
                    
                    if not shape_override or shape_override == "DEFAULT":
                        if len(leader_blueprint.get("path", [])) <= 2:
                            shape_override = "CIRCLE" if leader_blueprint.get("loop") else "PATROL"
                            
                    if shape_override and shape_override != "DEFAULT":
                        import copy
                        motion_override = copy.deepcopy(motion)
                        motion_override["pattern"] = shape_override
                        leader_blueprint["path"] = _build_ai_enemy_points(
                            style, motion_override, path_metadata[0]["spawn"], player, center, extents, path_metadata[0]["side"], 0, rng, __import__('mathutils').Vector((0,0,0))
                        )
                        if shape_override in {"CIRCLE", "OVAL", "FIGURE8", "ORBIT", "EDGE_ORBIT", "SPIRAL"}:
                            leader_blueprint["loop"] = True
                            
                    size_reference = leader_blueprint.get("size_reference")
                    if not size_reference:
                        import re
                        match = re.search(r'サイズ[はを]?([a-zA-Z0-9_-]+)と[同合]', self.motion_prompt)
                        if match:
                            size_reference = match.group(1)
                        elif "EnemyPath" in self.motion_prompt:
                            size_reference = "EnemyPath"
                            
                    ref_obj = None
                    if size_reference:
                        ref_obj = scene.objects.get(size_reference)
                        if not ref_obj:
                            for o in scene.objects:
                                if size_reference in o.name:
                                    ref_obj = o
                                    break
                    
                    if not ref_obj and path_metadata:
                        ref_obj = path_metadata[0]["obj"]
                        
                    if ref_obj:
                        points = leader_blueprint.get("path", [])
                        if ref_obj and points and len(points) > 1:
                            min_x = min(p.x for p in points)
                            max_x = max(p.x for p in points)
                            min_y = min(p.y for p in points)
                            max_y = max(p.y for p in points)
                            min_z = min(p.z for p in points)
                            max_z = max(p.z for p in points)
                            
                            size_x = max(max_x - min_x, 0.1)
                            size_y = max(max_y - min_y, 0.1)
                            size_z = max(max_z - min_z, 0.1)
                            
                            size_multiplier = 1.0
                            if _prompt_has(self.motion_prompt, "大き", "長く", "長め", "でかく"):
                                size_multiplier = 1.5
                            elif _prompt_has(self.motion_prompt, "小さ", "短く", "短め"):
                                size_multiplier = 0.66
                                
                            ref_size = ref_obj.dimensions
                            max_ref = max(ref_size.x, ref_size.y, ref_size.z) * size_multiplier
                            max_cur = max(size_x, size_y, size_z)
                            
                            if max_cur > 0 and max_ref > 0:
                                scale = max_ref / max_cur
                                start_pt = points[0].copy()
                                for j in range(len(points)):
                                    points[j] = start_pt + (points[j] - start_pt) * scale
                                leader_blueprint["path"] = points
            except Exception as exc:
                self.report({'ERROR'}, f"Ollama再生成に失敗しました: {exc}")
                
        for i, meta in enumerate(path_metadata):
            path_obj = meta["obj"]
            spawn = meta["spawn"]
            side = meta["side"]
            
            if leader_blueprint:
                try:
                    l_obj = path_metadata[0]["obj"]
                    m_obj = meta["obj"]
                    l_start = l_obj.matrix_world @ l_obj.data.splines[0].bezier_points[0].co
                    m_start = m_obj.matrix_world @ m_obj.data.splines[0].bezier_points[0].co
                    offset = m_start - l_start
                except:
                    offset = meta["spawn"] - path_metadata[0]["spawn"]
                
                if len(path_metadata) > 1:
                    xy_override = None
                    if _prompt_has(self.motion_prompt, "V字", "デルタ", "ブルーインパルス", "斜め", "v-formation", "delta"):
                        if len(leader_blueprint["path"]) >= 2:
                            forward = (leader_blueprint["path"][1] - leader_blueprint["path"][0]).normalized()
                        else:
                            forward = __import__('mathutils').Vector((0, -1, 0))
                        right = __import__('mathutils').Vector((forward.y, -forward.x, 0.0))
                        row = (i + 1) // 2
                        side_mult = -1.0 if i % 2 != 0 else 1.0
                        spacing = 3.0
                        xy_override = right * (side_mult * row * spacing) - forward * (row * spacing)
                    elif _prompt_has(self.motion_prompt, "まとめ", "集団", "編隊", "隊列", "一緒"):
                        columns = max(1, __import__('math').ceil(__import__('math').sqrt(len(path_metadata))))
                        rows = max(1, __import__('math').ceil(len(path_metadata) / columns))
                        spacing = 2.5
                        col = i % columns
                        row = i // columns
                        xy_override = __import__('mathutils').Vector((
                            (col - (columns - 1) * 0.5) * spacing,
                            (row - (rows - 1) * 0.5) * spacing,
                            0.0
                        ))
                        
                    if xy_override is not None:
                        offset.x = xy_override.x
                        offset.y = xy_override.y
                        
                    if _prompt_has(self.motion_prompt, "上下に分", "上下に置", "上下に並", "縦に", "上に", "下に", "縦並", "上下"):
                        stack_spacing = 2.5
                        offset.z = (i - (len(path_metadata) - 1) * 0.5) * stack_spacing
                points = [p + offset for p in leader_blueprint["path"]]
                
                # 同期: パスの始点に敵本体を移動させる
                if meta.get("spawn_obj"):
                    meta["spawn_obj"].location = points[0].copy()
                    
                loop = leader_blueprint["loop"]
                speed = leader_blueprint["speed"]
                handle_type = leader_blueprint["handle_type"]
            else:
                points = _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, 0, rng, __import__('mathutils').Vector((0,0,0)))
                loop = motion["loop"] if motion.get("loop") is not None else style == 'PATROL'
                speed = 0.050 * speed_mult
                handle_type = 'VECTOR' if motion.get("pattern") == "EDGE_ORBIT" and motion.get("respect_bounds") else 'AUTO'
                
            if path_obj.type == 'CURVE' and points:
                curve = path_obj.data
                spline = curve.splines[0] if curve.splines else curve.splines.new('BEZIER')
                if len(spline.bezier_points) < len(points):
                    spline.bezier_points.add(len(points) - len(spline.bezier_points))
                elif len(spline.bezier_points) > len(points):
                    curve.splines.remove(spline)
                    spline = curve.splines.new('BEZIER')
                    spline.bezier_points.add(len(points) - 1)
                
                spline.use_cyclic_u = loop
                for point, co in zip(spline.bezier_points, points):
                    point.co = co - path_obj.location
                    point.handle_left_type = handle_type
                    point.handle_right_type = handle_type
                
                path_obj.enemy_path_loop = loop
                path_obj.enemy_path_speed = speed
                path_obj["myaddon_ai_motion_prompt"] = self.motion_prompt
                updated_count += 1
                
        if updated_count > 0:
            if self.motion_prompt:
                msg = scene.myaddon_ai_enemy_chat_history.add()
                msg.role = "USER"
                msg.content = f"(個別編集) {self.motion_prompt}"
                msg.seed = self.seed
                scene.myaddon_ai_enemy_prompt = ""

            msg_ai = scene.myaddon_ai_enemy_chat_history.add()
            msg_ai.role = "AI"
            msg_ai.snapshot = _save_ai_enemy_snapshot(scene)
            if ollama_used:
                msg_ai.content = f"{updated_count}個のパスを再生成しました (Ollama最適化済)"
                msg_ai.gemini_ratio = 100
            else:
                msg_ai.content = f"{updated_count}個のパスを再生成しました (内蔵AI)"
                msg_ai.gemini_ratio = 0
                
        self.report({'INFO'}, f"{updated_count}個のパスを個別に再生成しました")
        return {'FINISHED'}



def _point_inside_local_box(local_point, min_point, max_point):
    return (
        min_point.x < local_point.x < max_point.x and
        min_point.y < local_point.y < max_point.y and
        min_point.z < local_point.z < max_point.z
    )

def _push_out_of_obstacles(point, scene):
    import mathutils
    from mathutils import Vector
    new_point = point.copy()
    
    # 複数回押し出し処理を行う（障害物が重なっている場合のため）
    for _ in range(3):
        collided = False
        for obj in scene.objects:
            if obj.type != "MESH" or obj.name.startswith("StageBounds"):
                continue
            if getattr(obj, "game_obj_type", "NONE") != "OBSTACLE":
                continue
                
            try:
                local_point = obj.matrix_world.inverted() @ new_point
            except Exception:
                continue
                
            collider = getattr(obj, "collider", "None")
            if collider in {"Box", "BOX"}:
                center = Vector(getattr(obj, "collider_center", (0.0, 0.0, 0.0)))
                size = Vector(getattr(obj, "collider_size", (0.0, 0.0, 0.0)))
                half_size = size * 0.5
                min_p = center - half_size
                max_p = center + half_size
                
                if _point_inside_local_box(local_point, min_p, max_p):
                    # 箱の中から外側への最短距離（6方向）を計算して押し出す
                    distances = [
                        (Vector((1, 0, 0)), max_p.x - local_point.x),
                        (Vector((-1, 0, 0)), local_point.x - min_p.x),
                        (Vector((0, 1, 0)), max_p.y - local_point.y),
                        (Vector((0, -1, 0)), local_point.y - min_p.y),
                        (Vector((0, 0, 1)), max_p.z - local_point.z),
                        (Vector((0, 0, -1)), local_point.z - min_p.z),
                    ]
                    # 一番近い面へ押し出す
                    best_dir, best_dist = min(distances, key=lambda x: x[1])
                    local_point += best_dir * (best_dist + 0.5) # 0.5余裕を持たせる
                    new_point = obj.matrix_world @ local_point
                    collided = True
            else:
                # bounding box fallback
                if len(obj.bound_box) > 0:
                    corners = [Vector(corner) for corner in obj.bound_box]
                    min_p = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
                    max_p = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
                    
                    if _point_inside_local_box(local_point, min_p, max_p):
                        distances = [
                            (Vector((1, 0, 0)), max_p.x - local_point.x),
                            (Vector((-1, 0, 0)), local_point.x - min_p.x),
                            (Vector((0, 1, 0)), max_p.y - local_point.y),
                            (Vector((0, -1, 0)), local_point.y - min_p.y),
                            (Vector((0, 0, 1)), max_p.z - local_point.z),
                            (Vector((0, 0, -1)), local_point.z - min_p.z),
                        ]
                        best_dir, best_dist = min(distances, key=lambda x: x[1])
                        local_point += best_dir * (best_dist + 0.5)
                        new_point = obj.matrix_world @ local_point
                        collided = True
        
        if not collided:
            break
            
    return new_point

def _sanitize_path_distances(points):
    import mathutils
    from mathutils import Vector
    if not points:
        return []
        
    sanitized = [points[0]]
    for p in points[1:]:
        dist = (p - sanitized[-1]).length
        if dist > 0.5: # 近すぎるポイントを排除
            sanitized.append(p)
            
    return sanitized


class MYADDON_OT_snap_all_enemies_to_ground(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_snap_all_enemies_to_ground"
    bl_label = "全VF3敵を山肌標高の上に直接着地"
    bl_description = "配置されたすべてのVF3敵を地上の山モデル(立方体/Ohirayama)の表面に一瞬で直接吸着・着地させます"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        scene = context.scene
        terrain_objs = [
            obj for obj in scene.objects 
            if obj.type == 'MESH' and obj.name != "StageBounds" and not obj.name.startswith("AIEnemy") and not obj.name.startswith("Enemy") and (
                any(k in obj.name.lower() for k in ["terrain", "ground", "mountain", "ohirayama", "stage", "field", "map", "立方体"]) 
                or (hasattr(obj.data, "vertices") and len(obj.data.vertices) >= 50)
            )
        ]

        snapped_count = 0
        for obj in scene.objects:
            if getattr(obj, "game_obj_type", "NONE") == "ENEMY" or obj.name.startswith("AIEnemy"):
                clean_type = str(getattr(obj, "enemy_type", "VF3")).upper().replace("-", "").replace("_", "").replace(" ", "")
                if "VF3" in clean_type or "GROUND" in clean_type or "TANK" in clean_type or "LAND" in clean_type or True:
                    world_pos = obj.matrix_world.translation.copy()
                    sx, sy, sz = _snap_point_to_terrain(world_pos, terrain_objs)
                    target_world_pos = Vector((sx, sy, sz))
                    
                    if obj.parent:
                        obj.location = obj.parent.matrix_world.inverted() @ target_world_pos
                    else:
                        obj.location = target_world_pos

                    obj.enemy_path_id = ""
                    snapped_count += 1

        self.report({'INFO'}, f"{snapped_count}体のVF3敵を山肌標高の上に直接着地させました。")
        return {'FINISHED'}


classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
    MYADDON_OT_validate_scene,
    MYADDON_OT_playtest_game,
    MYADDON_OT_apply_active_properties_to_selection,
    MYADDON_OT_ai_chat_clear,
    MYADDON_OT_ai_chat_revert,
    MYADDON_OT_ai_generate_level_obstacles,
    MYADDON_OT_randomize_ai_level_seed,
    MYADDON_OT_ai_edit_selected_obstacle,
    MYADDON_OT_randomize_ai_seed,
    MYADDON_OT_ai_enemy_chat_clear,
    MYADDON_OT_ai_enemy_chat_revert,
    MYADDON_OT_ai_generate_enemy_plan,
    MYADDON_OT_ai_edit_selected_enemy_path,
    MYADDON_OT_create_enemy_path,
    MYADDON_OT_assign_selected_enemy_path,
    MYADDON_OT_assign_selected_reinforcement_trigger,
    MYADDON_OT_assign_ai_reinforcement_trigger,
    MYADDON_OT_create_stage_bounds,
    MYADDON_OT_create_spawn_point,
    MYADDON_OT_snap_all_enemies_to_ground,
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

