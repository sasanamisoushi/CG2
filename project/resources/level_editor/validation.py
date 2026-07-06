import math

import bpy
from mathutils import Vector, geometry


TIMER_INTERVAL = 0.5
_TIMER_KEY = "cg2_level_editor_validation_timer"
_EPSILON = 0.0001
_TRANSFORM_EPSILON = 0.0001
_RESPAWN_DUPLICATE_DISTANCE = 0.01
_FAR_COORDINATE_LIMIT = 1000.0
_VALID_COLLIDERS = {"None", "Box", "BOX"}
_INVALID_FILENAME_CHARS = '<>:"/\\|?*'
_RESERVED_WINDOWS_FILENAMES = {
    "CON",
    "PRN",
    "AUX",
    "NUL",
    "COM1",
    "COM2",
    "COM3",
    "COM4",
    "COM5",
    "COM6",
    "COM7",
    "COM8",
    "COM9",
    "LPT1",
    "LPT2",
    "LPT3",
    "LPT4",
    "LPT5",
    "LPT6",
    "LPT7",
    "LPT8",
    "LPT9",
}

_CHECK_ITEMS = (
    ("export_targets", "出力対象の有無"),
    ("player_spawn", "PLAYER配置"),
    ("enemy_respawn_presence", "敵リスポーン地点配置"),
    ("stage_bounds_presence", "StageBounds配置"),
    ("spawn_inside_stage_bounds", "スポーンがStageBounds内にある"),
    ("spawn_buried", "リスポーン地点の埋まり"),
    ("respawn_duplicates", "リスポーン地点の重複"),
    ("base_name_duplicates", "名前ベース重複"),
    ("parenting", "親子関係"),
    ("path_id_duplicates", "飛行パスID重複"),
    ("path_points", "飛行パス制御点数"),
    ("path_speed", "飛行パス速度"),
    ("path_distance", "飛行パス距離"),
    ("path_far_coordinates", "飛行パスの遠すぎる座標"),
    ("unused_paths", "未使用の飛行パス"),
    ("path_reference", "敵から参照する飛行パス"),
    ("collider_type", "コライダー種別"),
    ("collider_size", "Boxコライダーサイズ"),
    ("aabb_rotation", "AABB想定Boxコライダー回転"),
    ("negative_scale", "マイナススケール"),
    ("far_coordinates", "異常に遠い座標"),
    ("empty_mesh", "空っぽのメッシュ"),
    ("filename", "ファイル名禁則文字"),
    ("unapplied_transform", "トランスフォーム未適用"),
    ("uv_map", "UVマップ"),
    ("ngon", "Nゴン"),
    ("material", "マテリアル割り当て"),
    ("category", "ゲーム用種類設定"),
    ("obstacle_type", "障害物メッシュ設定"),
    ("obstacle_collider", "障害物コライダー設定"),
    ("enemy_type", "敵タイプ設定"),
    ("reinforcement_trigger", "増援トリガー設定"),
    ("scale_zero", "0に近いスケール"),
    ("stage_bounds_category", "StageBounds種類設定"),
    ("other", "その他"),
)

_CHECK_LEVELS = {
    "OK": 0,
    "WARNING": 1,
    "ERROR": 2,
}


def _scene_objects(scene):
    return [obj for obj in scene.objects if obj.type in {"MESH", "EMPTY"}]


def _is_enemy_path_object(obj):
    if obj.type != "CURVE":
        return False
    path_id = getattr(obj, "enemy_path_id", "None")
    return path_id != "None" or obj.name.startswith("EnemyPath")


def _get_enemy_path_id(obj):
    path_id = getattr(obj, "enemy_path_id", "None")
    if path_id != "None":
        return path_id
    return _base_name(obj)


def _count_curve_points(obj):
    count = 0
    for spline in obj.data.splines:
        if spline.type == "BEZIER":
            count += len(spline.bezier_points)
        else:
            count += len(spline.points)
    return count


def _base_name(obj):
    return obj.name.split(".", 1)[0]


def _has_near_zero(values):
    return any(abs(value) <= _EPSILON for value in values)


def _is_near(value, target):
    return abs(value - target) <= _TRANSFORM_EPSILON


def _has_far_coordinate(point):
    return (
        abs(point.x) > _FAR_COORDINATE_LIMIT or
        abs(point.y) > _FAR_COORDINATE_LIMIT or
        abs(point.z) > _FAR_COORDINATE_LIMIT
    )


def _has_negative_scale(obj):
    return (
        obj.scale.x < -_EPSILON or
        obj.scale.y < -_EPSILON or
        obj.scale.z < -_EPSILON
    )


def _has_world_rotation(obj):
    try:
        _translation, rotation, _scale = obj.matrix_world.decompose()
    except Exception:
        return False
    return rotation.angle > _TRANSFORM_EPSILON


def _names(objects):
    return ", ".join(obj.name for obj in objects)


def _spawn_world_position(obj):
    if obj.type == "MESH" and getattr(obj, "game_obj_type", "NONE") == "ENEMY":
        local_center = sum((Vector(corner) for corner in obj.bound_box), Vector()) / 8.0
        return obj.matrix_world @ local_center

    return obj.matrix_world.translation


def _is_point_inside_stage_bounds(point, stage_bounds):
    try:
        local_point = stage_bounds.matrix_world.inverted() @ point
    except Exception:
        return False

    corners = [Vector(corner) for corner in stage_bounds.bound_box]
    min_x = min(corner.x for corner in corners) - _EPSILON
    min_y = min(corner.y for corner in corners) - _EPSILON
    min_z = min(corner.z for corner in corners) - _EPSILON
    max_x = max(corner.x for corner in corners) + _EPSILON
    max_y = max(corner.y for corner in corners) + _EPSILON
    max_z = max(corner.z for corner in corners) + _EPSILON

    return (
        min_x <= local_point.x <= max_x and
        min_y <= local_point.y <= max_y and
        min_z <= local_point.z <= max_z
    )


def _is_stage_bounds_object(obj):
    return _base_name(obj).startswith("StageBounds")


def _external_model_file(obj):
    model_file = str(getattr(obj, "game_model_file", "")).strip()
    if model_file:
        return model_file.replace("\\", "/")
    return ""


def _is_individual_mesh_export_target(obj):
    if obj.type != "MESH":
        return False
    if _is_stage_bounds_object(obj):
        return False
    if _external_model_file(obj):
        return False

    return getattr(obj, "game_obj_type", "NONE") != "ENEMY"


def _invalid_filename_reasons(name):
    reasons = []
    raw_name = str(name)
    stem = raw_name.strip()

    invalid_chars = sorted({char for char in raw_name if char in _INVALID_FILENAME_CHARS})
    if invalid_chars:
        reasons.append("禁則文字 " + " ".join(invalid_chars))

    if any(ord(char) < 32 for char in raw_name):
        reasons.append("制御文字")

    if not stem:
        reasons.append("空の名前")
    elif raw_name.endswith((" ", ".")):
        reasons.append("末尾のスペースまたはドット")

    reserved_check = stem.rstrip(" .").split(".", 1)[0].upper()
    if reserved_check in _RESERVED_WINDOWS_FILENAMES:
        reasons.append("Windows予約名")

    return reasons


def _describe_unapplied_transform(obj):
    _translation, rotation, scale = obj.matrix_basis.decompose()
    parts = []

    if rotation.angle > _TRANSFORM_EPSILON:
        parts.append("回転")

    if not (
        _is_near(scale.x, 1.0) and
        _is_near(scale.y, 1.0) and
        _is_near(scale.z, 1.0)
    ):
        parts.append("スケール")

    return " / ".join(parts)


def _has_missing_uv_map(obj):
    if obj.type != "MESH" or _is_stage_bounds_object(obj):
        return False

    mesh = obj.data
    return len(mesh.polygons) > 0 and len(mesh.uv_layers) == 0


def _has_ngon(obj):
    return obj.type == "MESH" and any(len(polygon.vertices) > 4 for polygon in obj.data.polygons)


def _has_unassigned_material(obj):
    if obj.type != "MESH" or _is_stage_bounds_object(obj):
        return False

    mesh = obj.data
    if len(mesh.polygons) == 0:
        return False

    if len(obj.material_slots) == 0:
        return True

    for polygon in mesh.polygons:
        if polygon.material_index >= len(obj.material_slots):
            return True
        if obj.material_slots[polygon.material_index].material is None:
            return True

    return False


def _curve_control_points(obj, spline):
    if spline.type == "BEZIER":
        return [obj.matrix_world @ point.co for point in spline.bezier_points]

    points = []
    for point in spline.points:
        w = point.co.w if point.co.w != 0.0 else 1.0
        points.append(obj.matrix_world @ Vector((point.co.x / w, point.co.y / w, point.co.z / w)))
    return points


def _get_curve_sample_points(obj):
    points = []

    for spline in obj.data.splines:
        if spline.type == "BEZIER":
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
                points.extend(obj.matrix_world @ sample for sample in samples)
        else:
            points.extend(_curve_control_points(obj, spline))

    return points


def _curve_has_zero_control_segment(obj):
    for spline in obj.data.splines:
        points = _curve_control_points(obj, spline)
        if len(points) < 2:
            continue

        segment_count = len(points) if getattr(spline, "use_cyclic_u", False) else len(points) - 1
        for index in range(segment_count):
            if (points[(index + 1) % len(points)] - points[index]).length <= _EPSILON:
                return True

    return False


def _curve_total_length(points):
    total = 0.0
    for index in range(len(points) - 1):
        total += (points[index + 1] - points[index]).length
    return total


def _validate_flight_path_distances(errors, warnings, path):
    sample_points = _get_curve_sample_points(path)
    if len(sample_points) >= 2 and _curve_total_length(sample_points) <= _EPSILON:
        errors.append(f"{path.name}: 飛行パスの距離が0です。制御点を離してください。")
        return

    if _curve_has_zero_control_segment(path):
        warnings.append(f"{path.name}: 飛行パスに距離0の制御点区間があります。")


def _point_inside_local_box(local_point, min_point, max_point):
    return (
        min_point.x + _EPSILON < local_point.x < max_point.x - _EPSILON and
        min_point.y + _EPSILON < local_point.y < max_point.y - _EPSILON and
        min_point.z + _EPSILON < local_point.z < max_point.z - _EPSILON
    )


def _is_spawn_inside_obstacle(spawn_obj, obstacle):
    if obstacle.type != "MESH" or _is_stage_bounds_object(obstacle):
        return False

    if getattr(obstacle, "game_obj_type", "NONE") != "OBSTACLE":
        return False

    try:
        local_point = obstacle.matrix_world.inverted() @ _spawn_world_position(spawn_obj)
    except Exception:
        return False

    collider = getattr(obstacle, "collider", "None")
    if collider in {"Box", "BOX"}:
        center = Vector(getattr(obstacle, "collider_center", (0.0, 0.0, 0.0)))
        size = Vector(getattr(obstacle, "collider_size", (0.0, 0.0, 0.0)))
        half_size = size * 0.5
        min_point = center - half_size
        max_point = center + half_size
        return _point_inside_local_box(local_point, min_point, max_point)

    if len(obstacle.bound_box) == 0:
        return False

    corners = [Vector(corner) for corner in obstacle.bound_box]
    min_point = Vector((
        min(corner.x for corner in corners),
        min(corner.y for corner in corners),
        min(corner.z for corner in corners),
    ))
    max_point = Vector((
        max(corner.x for corner in corners),
        max(corner.y for corner in corners),
        max(corner.z for corner in corners),
    ))
    return _point_inside_local_box(local_point, min_point, max_point)


def _validate_spawn_points_inside_obstacles(errors, spawn_objects, obstacles, label):
    for spawn in spawn_objects:
        buried_in = [
            obstacle.name
            for obstacle in obstacles
            if obstacle != spawn and _is_spawn_inside_obstacle(spawn, obstacle)
        ]
        if buried_in:
            errors.append(f"{spawn.name}: {label}が障害物に埋まっています: {', '.join(buried_in)}")


def _validate_respawn_duplicates(errors, spawn_objects, label):
    for index, current in enumerate(spawn_objects):
        current_position = _spawn_world_position(current)
        for other in spawn_objects[index + 1:]:
            other_position = _spawn_world_position(other)
            if (current_position - other_position).length <= _RESPAWN_DUPLICATE_DISTANCE:
                errors.append(f"{label}が重複しています: {current.name}, {other.name}")


def _validate_spawn_points_inside_stage_bounds(errors, spawn_objects, stage_bounds, label):
    if not stage_bounds:
        return

    for obj in spawn_objects:
        position = _spawn_world_position(obj)
        if any(_is_point_inside_stage_bounds(position, bounds) for bounds in stage_bounds):
            continue

        errors.append(f"{obj.name}: {label}がStageBoundsの外にあります。")


def _validate_base_name_duplicates(warnings, objects):
    names_by_base = {}
    for obj in objects:
        base_name = _base_name(obj)
        names_by_base.setdefault(base_name, []).append(obj.name)

    for base_name, names in names_by_base.items():
        if len(names) <= 1:
            continue
        warnings.append(f"名前のベースが重複しています: {base_name} ({', '.join(names)})")


def _is_unset_text(value):
    return value is None or str(value).strip() in {"", "None"}


def _classify_validation_message(message):
    if "出力対象" in message:
        return "export_targets"
    if "自機スポーン" in message and "StageBoundsの外" in message:
        return "spawn_inside_stage_bounds"
    if "敵リスポーン地点" in message and "StageBoundsの外" in message:
        return "spawn_inside_stage_bounds"
    if "障害物に埋まっています" in message:
        return "spawn_buried"
    if "敵リスポーン地点が重複しています" in message:
        return "respawn_duplicates"
    if message.startswith("PLAYER "):
        return "player_spawn"
    if "敵リスポーン地点がありません" in message:
        return "enemy_respawn_presence"
    if message.startswith("StageBounds が"):
        return "stage_bounds_presence"
    if "名前のベースが重複" in message:
        return "base_name_duplicates"
    if "親子関係" in message:
        return "parenting"
    if "飛行パスIDが重複" in message:
        return "path_id_duplicates"
    if "飛行パスには制御点" in message:
        return "path_points"
    if "飛行パスの速度" in message:
        return "path_speed"
    if "飛行パスの距離が0" in message or "距離0の制御点区間" in message:
        return "path_distance"
    if "飛行パスに絶対値" in message:
        return "path_far_coordinates"
    if "使われていない飛行パス" in message:
        return "unused_paths"
    if "飛行パスID" in message and "見つかりません" in message:
        return "path_reference"
    if "collider の値" in message or "collider.type が未対応" in message:
        return "collider_type"
    if "Box コライダーのサイズ" in message or "Box collider の size" in message:
        return "collider_size"
    if "AABB想定" in message:
        return "aabb_rotation"
    if "マイナススケール" in message:
        return "negative_scale"
    if "絶対値" in message:
        return "far_coordinates"
    if "空っぽのメッシュ" in message:
        return "empty_mesh"
    if "ファイル名" in message:
        return "filename"
    if "トランスフォームが未適用" in message:
        return "unapplied_transform"
    if "UVマップ" in message:
        return "uv_map"
    if "Nゴン" in message:
        return "ngon"
    if "マテリアル" in message:
        return "material"
    if "種類が未設定" in message or "category が" in message:
        return "category"
    if "障害物は MESH" in message:
        return "obstacle_type"
    if "OBSTACLE にコライダー" in message:
        return "obstacle_collider"
    if "enemy_type" in message:
        return "enemy_type"
    if "増援トリガー" in message:
        return "reinforcement_trigger"
    if "スケールに 0 に近い軸" in message or "StageBounds のサイズ" in message:
        return "scale_zero"
    if "StageBounds は種類" in message:
        return "stage_bounds_category"
    return "other"


def _shorten_check_detail(text):
    max_length = 140
    if len(text) <= max_length:
        return text
    return text[:max_length - 3] + "..."


def _build_check_report_text(errors, warnings):
    check_state = {
        check_id: {
            "label": label,
            "level": "OK",
            "messages": [],
        }
        for check_id, label in _CHECK_ITEMS
    }

    def add_message(level, message):
        check_id = _classify_validation_message(message)
        state = check_state.get(check_id, check_state["other"])
        if _CHECK_LEVELS[level] > _CHECK_LEVELS[state["level"]]:
            state["level"] = level
        state["messages"].append(message)

    for message in errors:
        add_message("ERROR", message)
    for message in warnings:
        add_message("WARNING", message)

    lines = []
    for check_id, label in _CHECK_ITEMS:
        state = check_state[check_id]
        messages = state["messages"]
        detail = ""
        if messages:
            detail = messages[0]
            if len(messages) > 1:
                detail = f"{detail} ほか{len(messages) - 1}件"
            detail = _shorten_check_detail(detail)
        lines.append(f"{state['level']}\t{label}\t{detail}")

    return "\n".join(lines)


def build_check_report_text(errors, warnings):
    return _build_check_report_text(errors, warnings)


def validate_scene(scene):
    errors = []
    warnings = []
    objects = _scene_objects(scene)

    players = [obj for obj in objects if getattr(obj, "game_obj_type", "NONE") == "PLAYER"]
    enemies = [obj for obj in objects if getattr(obj, "game_obj_type", "NONE") == "ENEMY"]
    enemies_by_name = {obj.name: obj for obj in enemies}
    obstacles = [obj for obj in objects if getattr(obj, "game_obj_type", "NONE") == "OBSTACLE"]
    stage_bounds = [obj for obj in objects if _is_stage_bounds_object(obj)]
    paths = [obj for obj in scene.objects if _is_enemy_path_object(obj)]
    path_ids = {}
    referenced_path_ids = {
        getattr(obj, "enemy_path_id", "None")
        for obj in enemies
        if getattr(obj, "enemy_path_id", "None") != "None"
    }

    if not objects:
        errors.append("出力対象の MESH / EMPTY がありません。")

    if len(players) == 0:
        errors.append("PLAYER がありません。自機スポーンを1つ配置してください。")
    elif len(players) > 1:
        errors.append(f"PLAYER が複数あります: {_names(players)}")

    if len(enemies) == 0:
        warnings.append("敵リスポーン地点がありません。")

    if len(stage_bounds) == 0:
        warnings.append("StageBounds がありません。ステージ外への移動制限が効かない可能性があります。")
    elif len(stage_bounds) > 1:
        warnings.append(f"StageBounds が複数あります: {_names(stage_bounds)}")

    _validate_spawn_points_inside_stage_bounds(errors, players, stage_bounds, "自機スポーン")
    _validate_spawn_points_inside_stage_bounds(errors, enemies, stage_bounds, "敵リスポーン地点")
    _validate_spawn_points_inside_obstacles(errors, players, obstacles, "自機スポーン")
    _validate_spawn_points_inside_obstacles(errors, enemies, obstacles, "敵リスポーン地点")
    _validate_respawn_duplicates(errors, enemies, "敵リスポーン地点")
    _validate_base_name_duplicates(warnings, objects)

    for obj in scene.objects:
        if obj.parent and (obj.type in {"MESH", "EMPTY"} or _is_enemy_path_object(obj)):
            warnings.append(f"{obj.name}: 親子関係があります。ゲーム出力ではワールド座標で扱われるため、意図通りか確認してください。")

    for path in paths:
        path_id = _get_enemy_path_id(path)
        if path_id in path_ids:
            errors.append(f"飛行パスIDが重複しています: {path_id} ({path_ids[path_id].name}, {path.name})")
        else:
            path_ids[path_id] = path

        if _count_curve_points(path) < 2:
            errors.append(f"{path.name}: 飛行パスには制御点が2つ以上必要です。")

        if getattr(path, "enemy_path_speed", 0.0) <= 0.0:
            errors.append(f"{path.name}: 飛行パスの速度は 0 より大きくしてください。")

        _validate_flight_path_distances(errors, warnings, path)

        if any(_has_far_coordinate(point) for point in _get_curve_sample_points(path)):
            warnings.append(f"{path.name}: 飛行パスに絶対値 {_FAR_COORDINATE_LIMIT:g} を超える座標があります。")

        if path_id not in referenced_path_ids:
            warnings.append(f"{path.name}: どの敵リスポーン地点にも使われていない飛行パスです。")

    for obj in objects:
        category = getattr(obj, "game_obj_type", "NONE")
        collider = getattr(obj, "collider", "None")

        if collider not in _VALID_COLLIDERS:
            errors.append(f"{obj.name}: collider の値が不正です: {collider}")
        elif collider in {"Box", "BOX"}:
            size = getattr(obj, "collider_size", (0.0, 0.0, 0.0))
            if any(not math.isfinite(value) or value <= 0.0 for value in size):
                errors.append(f"{obj.name}: Box コライダーのサイズはすべて 0 より大きくしてください。")
            if not _is_stage_bounds_object(obj) and _has_world_rotation(obj):
                warnings.append(f"{obj.name}: AABB想定のBoxコライダーに回転があります。軸並行で使う場合は回転を0にしてください。")

        if _has_negative_scale(obj):
            warnings.append(f"{obj.name}: マイナススケールがあります。法線反転や当たり判定ずれに注意してください。")

        if _has_far_coordinate(obj.matrix_world.translation):
            warnings.append(f"{obj.name}: 絶対値 {_FAR_COORDINATE_LIMIT:g} を超える座標にあります。配置ミスでないか確認してください。")

        if obj.type == "MESH" and len(obj.data.vertices) == 0:
            warnings.append(f"{obj.name}: 空っぽのメッシュです（頂点数0）。")

        if _is_individual_mesh_export_target(obj):
            invalid_filename_reasons = _invalid_filename_reasons(obj.name)
            if invalid_filename_reasons:
                warnings.append(f"{obj.name}: ファイル名に使えない名前です（{', '.join(invalid_filename_reasons)}）。OBJ出力名では置換されます。")

            unapplied_transform = _describe_unapplied_transform(obj)
            if unapplied_transform:
                warnings.append(f"{obj.name}: トランスフォームが未適用です（{unapplied_transform}）。必要なら Ctrl+A で適用してください。")

            if _has_missing_uv_map(obj):
                warnings.append(f"{obj.name}: UVマップがありません。テクスチャ表示が崩れる可能性があります。")

            if _has_ngon(obj):
                warnings.append(f"{obj.name}: Nゴン（5頂点以上のポリゴン）があります。三角形または四角形に分割してください。")

        if _has_unassigned_material(obj):
            warnings.append(f"{obj.name}: マテリアルが割り当てられていない面があります。")

        if category == "NONE":
            if obj.type == "MESH":
                warnings.append(f"{obj.name}: 種類が未設定です。ゲーム側では障害物として扱われる可能性があります。")
            continue

        if category == "OBSTACLE" and obj.type != "MESH":
            warnings.append(f"{obj.name}: 障害物は MESH で配置してください。")

        if category == "OBSTACLE" and not obj.name.startswith("StageBounds"):
            if collider == "None":
                warnings.append(f"{obj.name}: OBSTACLE にコライダーが設定されていません。")

        if category == "ENEMY" and _is_unset_text(getattr(obj, "enemy_type", "None")):
            errors.append(f"{obj.name}: enemy_type が未設定です。")

        if category == "ENEMY":
            trigger_name = getattr(obj, "enemy_reinforcement_trigger_name", "")
            if not _is_unset_text(trigger_name):
                trigger_name = str(trigger_name).strip()
                if trigger_name == obj.name:
                    errors.append(f"{obj.name}: 増援トリガーに自分自身は指定できません。")
                elif trigger_name not in enemies_by_name:
                    errors.append(f"{obj.name}: 増援トリガー '{trigger_name}' が見つかりません。")

        if obj.type == "MESH":
            if obj.name.startswith("StageBounds"):
                size_values = (obj.dimensions.x, obj.dimensions.y, obj.dimensions.z)
                if _has_near_zero(size_values):
                    errors.append(f"{obj.name}: StageBounds のサイズに 0 に近い軸があります。")
            elif _has_near_zero((obj.scale.x, obj.scale.y, obj.scale.z)):
                errors.append(f"{obj.name}: スケールに 0 に近い軸があります。")

        if obj.name.startswith("StageBounds") and category != "OBSTACLE":
            warnings.append(f"{obj.name}: StageBounds は種類を障害物にしてください。")

        if category == "ENEMY":
            path_id = getattr(obj, "enemy_path_id", "None")
            if path_id != "None" and path_id not in path_ids:
                errors.append(f"{obj.name}: 飛行パスID '{path_id}' が見つかりません。")

    return errors, warnings


def _make_status(errors, warnings):
    if errors:
        return f"エラー {len(errors)}件 / 警告 {len(warnings)}件"
    if warnings:
        return f"警告 {len(warnings)}件"
    return "OK"


def validate_and_store(scene=None):
    if scene is None:
        scene = bpy.context.scene

    errors, warnings = validate_scene(scene)
    error_text = "\n".join(errors)
    warning_text = "\n".join(warnings)
    check_text = build_check_report_text(errors, warnings)
    status = _make_status(errors, warnings)

    if getattr(scene, "myaddon_validation_errors", "") != error_text:
        scene.myaddon_validation_errors = error_text
    if getattr(scene, "myaddon_validation_warnings", "") != warning_text:
        scene.myaddon_validation_warnings = warning_text
    if getattr(scene, "myaddon_validation_checks", "") != check_text:
        scene.myaddon_validation_checks = check_text
    if getattr(scene, "myaddon_validation_status", "") != status:
        scene.myaddon_validation_status = status

    return errors, warnings


def has_errors(scene=None):
    errors, _ = validate_and_store(scene)
    return len(errors) > 0


def run_auto_validation():
    scene = bpy.context.scene
    if scene and getattr(scene, "myaddon_auto_validation", True):
        validate_and_store(scene)
    return TIMER_INTERVAL


def register():
    bpy.types.Scene.myaddon_auto_validation = bpy.props.BoolProperty(
        name="自動バリデーション",
        default=True,
    )
    bpy.types.Scene.myaddon_validation_status = bpy.props.StringProperty(
        name="バリデーション状態",
        default="未チェック",
    )
    bpy.types.Scene.myaddon_validation_errors = bpy.props.StringProperty(
        name="バリデーションエラー",
        default="",
    )
    bpy.types.Scene.myaddon_validation_warnings = bpy.props.StringProperty(
        name="バリデーション警告",
        default="",
    )
    bpy.types.Scene.myaddon_validation_checks = bpy.props.StringProperty(
        name="バリデーション項目",
        default="",
    )

    previous_timer = bpy.app.driver_namespace.pop(_TIMER_KEY, None)
    if previous_timer and bpy.app.timers.is_registered(previous_timer):
        bpy.app.timers.unregister(previous_timer)

    bpy.app.driver_namespace[_TIMER_KEY] = run_auto_validation
    if not bpy.app.timers.is_registered(run_auto_validation):
        bpy.app.timers.register(run_auto_validation, first_interval=0.1)

    validate_and_store(bpy.context.scene)


def unregister():
    previous_timer = bpy.app.driver_namespace.pop(_TIMER_KEY, None)
    if previous_timer and bpy.app.timers.is_registered(previous_timer):
        bpy.app.timers.unregister(previous_timer)

    for prop_name in (
        "myaddon_validation_warnings",
        "myaddon_validation_errors",
        "myaddon_validation_checks",
        "myaddon_validation_status",
        "myaddon_auto_validation",
    ):
        if hasattr(bpy.types.Scene, prop_name):
            delattr(bpy.types.Scene, prop_name)
