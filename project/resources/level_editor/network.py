import bpy
import json
import math
import socket
from mathutils import Vector


UDP_IP = "127.0.0.1"
UDP_PORT = 50000
TIMER_INTERVAL = 0.25

sock = None
last_sent_json = None

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


def build_scene_data(scene):
    scene_data = {"objects": []}

    for obj in scene.objects:
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

        if hasattr(obj, "enemy_type") and obj.enemy_type != "None":
            obj_data["enemy"] = {"type": obj.enemy_type}

        scene_data["objects"].append(obj_data)

    return scene_data


def send_scene_realtime(*_):
    global sock, last_sent_json
    if sock is None:
        return

    try:
        json_str = json.dumps(build_scene_data(bpy.context.scene))
        if json_str == last_sent_json:
            return

        sock.sendto(json_str.encode("utf-8"), (UDP_IP, UDP_PORT))
        last_sent_json = json_str
    except Exception:
        pass


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
    global sock, last_sent_json
    unregister_realtime_sync()

    previous_socket = bpy.app.driver_namespace.pop(_SOCKET_KEY, None)
    if previous_socket:
        previous_socket.close()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    last_sent_json = None
    bpy.app.driver_namespace[_SOCKET_KEY] = sock
    register_realtime_sync()


def unregister():
    global sock, last_sent_json
    unregister_realtime_sync()
    active_socket = bpy.app.driver_namespace.pop(_SOCKET_KEY, None)
    if active_socket:
        active_socket.close()
    if sock and sock is not active_socket:
        sock.close()
    sock = None
    last_sent_json = None
