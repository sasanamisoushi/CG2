import bpy
import json
import math
import socket


UDP_IP = "127.0.0.1"
UDP_PORT = 50000
TIMER_INTERVAL = 0.25

sock = None


def build_scene_data(scene):
    scene_data = {"objects": []}

    for obj in scene.objects:
        if obj.type not in ["MESH", "EMPTY"]:
            continue

        trans, rot, scale = obj.matrix_local.decompose()
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


def send_scene_realtime(scene):
    global sock
    if sock is None:
        return

    try:
        json_str = json.dumps(build_scene_data(bpy.context.scene))
        sock.sendto(json_str.encode("utf-8"), (UDP_IP, UDP_PORT))
    except Exception:
        pass


def send_scene_timer():
    if sock is None:
        return None

    send_scene_realtime(bpy.context.scene)
    return TIMER_INTERVAL


def register_realtime_sync():
    if send_scene_realtime not in bpy.app.handlers.depsgraph_update_post:
        bpy.app.handlers.depsgraph_update_post.append(send_scene_realtime)

    if not bpy.app.timers.is_registered(send_scene_timer):
        bpy.app.timers.register(send_scene_timer, first_interval=0.1)


def unregister_realtime_sync():
    if send_scene_realtime in bpy.app.handlers.depsgraph_update_post:
        bpy.app.handlers.depsgraph_update_post.remove(send_scene_realtime)

    if bpy.app.timers.is_registered(send_scene_timer):
        bpy.app.timers.unregister(send_scene_timer)


def register():
    global sock
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    register_realtime_sync()


def unregister():
    global sock
    unregister_realtime_sync()
    if sock:
        sock.close()
        sock = None
