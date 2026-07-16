import sys

op_file = r"project\resources\level_editor\operators.py"
with open(op_file, "r", encoding="utf-8") as f:
    text = f.read()

helper_funcs = """
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

"""

if "def _push_out_of_obstacles" not in text:
    text = text.replace("classes = (", helper_funcs + "\nclasses = (")
    with open(op_file, "w", encoding="utf-8") as f:
        f.write(text)
    print("Injected helper_funcs successfully.")
else:
    print("helper_funcs already exist.")
