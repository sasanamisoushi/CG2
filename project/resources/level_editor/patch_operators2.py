import sys

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

chunk1_old = '''    return Vector((0.0, 0.0, 0.0)), Vector((18.0, 22.0, 10.0))


def _find_player_spawn_position'''

chunk1_new = '''    return Vector((0.0, 0.0, 0.0)), Vector((18.0, 22.0, 10.0))

def _get_target_bounds(scene, prompt=""):
    from mathutils import Vector
    if prompt:
        sorted_objs = sorted([o for o in scene.objects if o.type == 'MESH'], key=lambda o: len(o.name), reverse=True)
        for obj in sorted_objs:
            if obj.name in prompt:
                center = obj.matrix_world.translation.copy()
                extents = Vector((
                    max(abs(obj.scale.x) * 2.0, 5.0),
                    max(abs(obj.scale.y) * 2.0, 5.0),
                    max(abs(obj.scale.z) * 2.0, 5.0)
                ))
                return center, extents
                
    try:
        from . import operators
        return operators._stage_bounds_box(scene)
    except:
        return _stage_bounds_box(scene)


def _find_player_spawn_position'''

text = text.replace(chunk1_old, chunk1_new)

chunk2_old = '''def _build_ai_level_obstacles(context, obstacles_to_create, shape_type):
    import bpy
    scene = context.scene
    created_objs = []
    
    # 境界ボックスの取得
    from mathutils import Vector
    center = Vector((0,0,0))
    extents = Vector((20,20,20))
    for obj in scene.objects:
        if obj.name == "StageBounds":
            center = obj.location
            extents = Vector((obj.scale.x * 2.0, obj.scale.y * 2.0, obj.scale.z * 2.0))
            break'''

chunk2_new = '''def _build_ai_level_obstacles(context, obstacles_to_create, shape_type, prompt=""):
    import bpy
    scene = context.scene
    created_objs = []
    
    # 境界ボックスの取得
    from mathutils import Vector
    center, extents = _get_target_bounds(scene, prompt)'''

text = text.replace(chunk2_old, chunk2_new)

chunk3_old = '''        created_objs = _build_ai_level_obstacles(context, obstacles_to_create, shape)'''

chunk3_new = '''        created_objs = _build_ai_level_obstacles(context, obstacles_to_create, shape, prompt)'''

text = text.replace(chunk3_old, chunk3_new)

chunk4_old = '''        center = Vector((0,0,0))
        extents = Vector((20,20,20))
        try:
            from . import operators
            center, extents = operators._stage_bounds_box(scene)
        except:
            for obj in scene.objects:
                if obj.name == "StageBounds":
                    center = obj.location
                    extents = Vector((obj.scale.x * 2.0, obj.scale.y * 2.0, obj.scale.z * 2.0))
                    break'''

chunk4_new = '''        center, extents = _get_target_bounds(scene, prompt)'''

# There are two occurrences of chunk4_old, one in MYADDON_OT_ai_generate_level_obstacles and one in MYADDON_OT_ai_edit_selected_obstacle.
# In MYADDON_OT_ai_edit_selected_obstacle, we need to pass self.level_prompt instead of prompt.
# So we will do it with find and replace manually.

idx = text.find(chunk4_old)
if idx != -1:
    text = text[:idx] + chunk4_new + text[idx+len(chunk4_old):]
else:
    print("Failed to find chunk4_old 1")

idx2 = text.find(chunk4_old)
if idx2 != -1:
    text = text[:idx2] + '''        center, extents = _get_target_bounds(scene, self.level_prompt)''' + text[idx2+len(chunk4_old):]
else:
    print("Failed to find chunk4_old 2")

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)

print("SUCCESS")
