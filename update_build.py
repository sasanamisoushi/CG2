import sys

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

old_build = '''def _build_ai_level_obstacles(context, obstacles_to_create, shape_type):
    import bpy
    scene = context.scene
    created_objs = []
    
    col_name = "AI_Obstacles"
    obs_col = bpy.data.collections.get(col_name)
    if not obs_col:
        obs_col = bpy.data.collections.new(col_name)
        scene.collection.children.link(obs_col)
        
    for obs_data in obstacles_to_create:
        loc = obs_data["loc"]
        scale = obs_data["scale"]'''

new_build = '''def _build_ai_level_obstacles(context, obstacles_to_create, shape_type):
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
            break
    
    col_name = "AI_Obstacles"
    obs_col = bpy.data.collections.get(col_name)
    if not obs_col:
        obs_col = bpy.data.collections.new(col_name)
        scene.collection.children.link(obs_col)
        
    for obs_data in obstacles_to_create:
        loc = obs_data["loc"]
        scale = obs_data["scale"]
        
        # 1. 境界内チェック (Clamp to StageBounds)
        loc = _clamp_point_to_box(loc, center, extents, Vector((scale.x, scale.y, 0.0)))
        
        # 2. 衝突回避 (他障害物と被らないように押し出し)
        loc = _push_out_of_obstacles(loc, scene)'''

text = text.replace(old_build, new_build)

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)
