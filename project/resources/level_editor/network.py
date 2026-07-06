import bpy
import json
import math
import socket
from mathutils import Vector, geometry

import validation


UDP_IP = "127.0.0.1"
UDP_PORT = 50000
TIMER_INTERVAL = 0.25
MAX_UDP_PAYLOAD = 60000
INVALID_FILENAME_CHARS = '<>:"/\\|?*'

sock = None
last_sent_json = None
last_validation_error_print = None

_HANDLER_KEY = "cg2_level_editor_realtime_handler"
_TIMER_KEY = "cg2_level_editor_realtime_timer"
_SOCKET_KEY = "cg2_level_editor_realtime_socket"


def get_world_transform(obj):
    trans, rot, scale = obj.matrix_world.decompose()

    # 敵モデルをリスポーン地点として使う場合は、原点ではなく見た目の中心を送る。
    if obj.type == "MESH" and getattr(obj, "game_obj_type", "NONE") == "ENEMY":
        local_center = sum((Vector(corner) for corner in obj.bound_box), Vector()) / 8.0
        trans = obj.matrix_world @ local_center

    return trans, rot, scale


def is_enemy_path_object(obj):
    if obj.type != "CURVE":
        return False
    path_id = getattr(obj, "enemy_path_id", "None")
    return path_id != "None" or obj.name.startswith("EnemyPath")


def is_model_export_target(obj):
    if obj.type != "MESH":
        return False
    if obj.name.startswith("StageBounds"):
        return False
    if external_model_file(obj):
        return False

    category = getattr(obj, "game_obj_type", "NONE")
    return category != "ENEMY"


def external_model_file(obj):
    model_file = str(getattr(obj, "game_model_file", "")).strip()
    if model_file:
        return model_file.replace("\\", "/")
    return ""


def safe_filename_stem(name):
    stem = name.strip()
    for char in INVALID_FILENAME_CHARS:
        stem = stem.replace(char, "_")
    stem = "".join("_" if ord(char) < 32 else char for char in stem)
    stem = stem.strip(" .")
    return stem or "Mesh"


def build_model_filename_map(scene):
    model_filenames = {}
    used_filenames = set()

    for obj in scene.objects:
        if not is_model_export_target(obj):
            continue

        stem = safe_filename_stem(obj.name)
        filename = f"{stem}.obj"
        index = 2
        while filename.lower() in used_filenames:
            filename = f"{stem}_{index}.obj"
            index += 1

        used_filenames.add(filename.lower())
        model_filenames[obj.name] = filename

    return model_filenames


def get_enemy_path_id(obj):
    path_id = getattr(obj, "enemy_path_id", "None")
    if path_id != "None":
        return path_id
    return obj.name.split(".", 1)[0]


def is_unset_text(value):
    return value is None or str(value).strip() in {"", "None"}


def get_curve_world_points(obj):
    points = []

    def append_world_point(local_point):
        world_point = obj.matrix_world @ local_point
        if points:
            previous = Vector(points[-1])
            if (world_point - previous).length <= 0.0001:
                return
        points.append([world_point.x, world_point.y, world_point.z])

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
                for sample in samples:
                    append_world_point(sample)
        else:
            for point in spline.points:
                w = point.co.w if point.co.w != 0.0 else 1.0
                append_world_point(Vector((point.co.x / w, point.co.y / w, point.co.z / w)))

    return points


def build_scene_data(scene):
    errors, warnings = validation.validate_scene(scene)
    scene_data = {
        "objects": [],
        "paths": [],
        "validation": {
            "status": make_validation_status(errors, warnings),
            "errors": errors,
            "warnings": warnings,
            "checks": validation.build_check_report_text(errors, warnings),
        },
    }
    model_filenames = build_model_filename_map(scene)

    for obj in scene.objects:
        if is_enemy_path_object(obj):
            scene_data["paths"].append({
                "id": get_enemy_path_id(obj),
                "name": obj.name,
                "loop": bool(getattr(obj, "enemy_path_loop", False)),
                "speed": float(getattr(obj, "enemy_path_speed", 0.05)),
                "points": get_curve_world_points(obj),
            })
            continue

        if obj.type not in ["MESH", "EMPTY"]:
            continue

        trans, rot, scale = get_world_transform(obj)
        rot = rot.to_euler()
        rot.x, rot.y, rot.z = [math.degrees(v) for v in rot]

        obj_data = {
            "type": obj.type,
            "name": obj.name,
            "transform": {
                "translation": [trans.x, trans.y, trans.z],
                "rotation": [rot.x, rot.y, rot.z],
                "scale": [scale.x, scale.y, scale.z],
            },
            "vertices_count": len(obj.data.vertices) if obj.type == "MESH" else 0,
        }

        if hasattr(obj, "game_obj_type") and obj.game_obj_type != "NONE":
            obj_data["category"] = obj.game_obj_type

        external_model = external_model_file(obj)
        if external_model:
            obj_data["model"] = external_model
        elif obj.type == "MESH" and obj.name in model_filenames:
            obj_data["model"] = model_filenames[obj.name]

        if hasattr(obj, "enemy_type") and obj.enemy_type != "None":
            obj_data["enemy"] = {"type": obj.enemy_type}

        if getattr(obj, "game_obj_type", "NONE") == "ENEMY":
            path_id = getattr(obj, "enemy_path_id", "None")
            if path_id != "None":
                obj_data["path_id"] = path_id

            trigger_name = getattr(obj, "enemy_reinforcement_trigger_name", "")
            if not is_unset_text(trigger_name):
                obj_data["reinforcement"] = {
                    "trigger": str(trigger_name).strip(),
                    "delay": max(0, int(getattr(obj, "enemy_reinforcement_delay_frames", 0))),
                }

        scene_data["objects"].append(obj_data)

    return scene_data


def make_validation_status(errors, warnings):
    if errors:
        return f"エラー {len(errors)}件 / 警告 {len(warnings)}件"
    if warnings:
        return f"警告 {len(warnings)}件"
    return "OK"


def send_scene_realtime(*_):
    global sock, last_sent_json, last_validation_error_print
    if sock is None:
        return

    try:
        errors, _ = validation.validate_and_store(bpy.context.scene)
        if errors:
            error_text = "\n".join(errors)
            if error_text != last_validation_error_print:
                print("リアルタイム送信をスキップしました。バリデーションエラーがあります:")
                for message in errors:
                    print(f"  - {message}")
                last_validation_error_print = error_text
            return
        last_validation_error_print = None

        json_str = json.dumps(build_scene_data(bpy.context.scene), ensure_ascii=False, separators=(",", ":"))
        if json_str == last_sent_json:
            return

        payload = json_str.encode("utf-8")
        if len(payload) > MAX_UDP_PAYLOAD:
            print(f"リアルタイム送信をスキップしました。データが大きすぎます: {len(payload)} bytes")
            return

        sock.sendto(payload, (UDP_IP, UDP_PORT))
        last_sent_json = json_str
    except Exception as exc:
        print(f"リアルタイム送信に失敗しました: {exc}")


def send_scene_timer():
    if sock is None:
        return None

    send_scene_realtime(bpy.context.scene)
    return TIMER_INTERVAL


def register_realtime_sync():
    unregister_realtime_sync()

    if send_scene_realtime not in bpy.app.handlers.depsgraph_update_post:
        bpy.app.handlers.depsgraph_update_post.append(send_scene_realtime)

    if not bpy.app.timers.is_registered(send_scene_timer):
        bpy.app.timers.register(send_scene_timer, first_interval=0.1)

    bpy.app.driver_namespace[_HANDLER_KEY] = send_scene_realtime
    bpy.app.driver_namespace[_TIMER_KEY] = send_scene_timer


def unregister_realtime_sync():
    handlers = bpy.app.handlers.depsgraph_update_post
    previous_handler = bpy.app.driver_namespace.pop(_HANDLER_KEY, None)
    if previous_handler in handlers:
        handlers.remove(previous_handler)

    # アドオン再読み込み前の古い送信処理も停止する。
    for handler in list(handlers):
        if getattr(handler, "__name__", "") == "send_scene_realtime":
            old_globals = getattr(handler, "__globals__", {})
            old_socket = old_globals.get("sock")
            if old_socket:
                old_socket.close()
                old_globals["sock"] = None
            handlers.remove(handler)

    previous_timer = bpy.app.driver_namespace.pop(_TIMER_KEY, None)
    if previous_timer and bpy.app.timers.is_registered(previous_timer):
        bpy.app.timers.unregister(previous_timer)


def register():
    global sock, last_sent_json, last_validation_error_print
    unregister_realtime_sync()

    previous_socket = bpy.app.driver_namespace.pop(_SOCKET_KEY, None)
    if previous_socket:
        previous_socket.close()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    last_sent_json = None
    last_validation_error_print = None
    bpy.app.driver_namespace[_SOCKET_KEY] = sock
    register_realtime_sync()


def unregister():
    global sock, last_sent_json, last_validation_error_print
    unregister_realtime_sync()
    active_socket = bpy.app.driver_namespace.pop(_SOCKET_KEY, None)
    if active_socket:
        active_socket.close()
    if sock and sock is not active_socket:
        sock.close()
    sock = None
    last_sent_json = None
    last_validation_error_print = None
