import sys
import os

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

level_gen_code = '''
def _parse_ai_level_prompt(prompt):
    normalized = str(prompt or "").casefold()
    plan = {
        "style": "CITY",
        "shape": "CUBE",
        "density_multiplier": 1.0,
    }

    if _prompt_has(normalized, "迷路", "maze"):
        plan["style"] = "MAZE"
    elif _prompt_has(normalized, "防衛線", "防壁", "ライン", "defense", "line"):
        plan["style"] = "DEFENSE_LINE"
    elif _prompt_has(normalized, "アリーナ", "広場", "arena", "open"):
        plan["style"] = "ARENA"
    elif _prompt_has(normalized, "市街地", "街", "ビル", "city", "building"):
        plan["style"] = "CITY"

    if _prompt_has(normalized, "円柱", "柱", "cylinder", "pillar"):
        plan["shape"] = "CYLINDER"

    if _prompt_has(normalized, "たくさん", "大量", "多い", "many", "dense", "2倍", "２倍"):
        plan["density_multiplier"] = 2.0
    elif _prompt_has(normalized, "少し", "少ない", "まばら", "few", "sparse", "半分"):
        plan["density_multiplier"] = 0.5

    return plan

def _delete_ai_generated_obstacles(scene):
    import bpy
    to_delete = []
    for obj in scene.objects:
        if obj.get("ai_generated_obstacle"):
            to_delete.append(obj)
            
    if to_delete:
        bpy.ops.object.select_all(action='DESELECT')
        for obj in to_delete:
            obj.select_set(True)
        bpy.ops.object.delete()

def _build_ai_level_obstacles(context, obstacles_to_create, shape_type):
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
        scale = obs_data["scale"]
        
        if shape_type == "CYLINDER":
            bpy.ops.mesh.primitive_cylinder_add(location=loc)
        else:
            bpy.ops.mesh.primitive_cube_add(location=loc)
            
        obj = context.active_object
        obj.scale = scale
        obj["ai_generated_obstacle"] = True
        obj["game_obj_type"] = "OBSTACLE"
        
        for c in obj.users_collection:
            c.objects.unlink(obj)
        obs_col.objects.link(obj)
        
        created_objs.append(obj)
        
    return created_objs

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
            
        plan = _parse_ai_level_prompt(prompt)
        rng = random.Random(seed)
        
        center = Vector((0,0,0))
        extents = Vector((20,20,20))
        try:
            from . import operators
            center, extents = operators._stage_bounds_box(scene)
        except:
            for obj in scene.objects:
                if obj.name == "StageBounds":
                    center = obj.location
                    extents = Vector((obj.scale.x * 2.0, obj.scale.y * 2.0, obj.scale.z * 2.0))
                    break
        
        style = plan["style"]
        shape = plan["shape"]
        density = plan["density_multiplier"]
        
        obstacles_to_create = []
        
        if style == "CITY":
            base_count = int(16 * density)
            cols = max(2, int(math.sqrt(base_count)))
            rows = max(2, int(base_count / cols))
            spacing_x = (extents.x * 2 * 0.8) / cols
            spacing_y = (extents.y * 2 * 0.8) / rows
            
            for r in range(rows):
                for c in range(cols):
                    if rng.random() > 0.3:
                        lx = center.x - extents.x*0.8 + c * spacing_x + rng.uniform(-2, 2)
                        ly = center.y - extents.y*0.8 + r * spacing_y + rng.uniform(-2, 2)
                        obstacles_to_create.append({
                            "loc": Vector((lx, ly, 0)),
                            "scale": Vector((rng.uniform(1.0, 3.0), rng.uniform(1.0, 3.0), rng.uniform(4.0, 12.0)))
                        })
        elif style == "DEFENSE_LINE":
            base_count = int(8 * density)
            spacing = (extents.x * 2 * 0.9) / max(1, base_count)
            for i in range(base_count):
                lx = center.x - extents.x*0.9 + i * spacing
                ly = center.y + extents.y * rng.uniform(0.1, 0.3)
                obstacles_to_create.append({
                    "loc": Vector((lx, ly + rng.uniform(-1, 1), 0)),
                    "scale": Vector((rng.uniform(2.0, 6.0), rng.uniform(1.0, 2.0), rng.uniform(2.0, 4.0)))
                })
        elif style == "MAZE":
            base_count = int(12 * density)
            for i in range(base_count):
                lx = center.x + rng.uniform(-extents.x*0.8, extents.x*0.8)
                ly = center.y + rng.uniform(-extents.y*0.8, extents.y*0.8)
                is_horizontal = rng.random() > 0.5
                sx = rng.uniform(6.0, 15.0) if is_horizontal else rng.uniform(1.0, 2.0)
                sy = rng.uniform(1.0, 2.0) if is_horizontal else rng.uniform(6.0, 15.0)
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((sx, sy, rng.uniform(3.0, 6.0)))
                })
        elif style == "ARENA":
            base_count = int(12 * density)
            for i in range(base_count):
                angle = (i / base_count) * 2 * math.pi
                r = min(extents.x, extents.y) * 0.7
                lx = center.x + math.cos(angle) * r
                ly = center.y + math.sin(angle) * r
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((2.0, 2.0, rng.uniform(3.0, 8.0)))
                })
                
        created_objs = _build_ai_level_obstacles(context, obstacles_to_create, shape)
        self.report({'INFO'}, f"{len(created_objs)}個のAI障害物を配置しました")
        
        return {'FINISHED'}
'''

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

if 'class MYADDON_OT_ai_generate_level_obstacles' in text:
    lines = text.split('\n')
    new_lines = []
    skip = False
    for line in lines:
        if line.startswith('def _parse_ai_level_prompt'):
            skip = True
        if line.startswith('class MYADDON_OT_ai_generate_enemy_plan'):
            skip = False
        if not skip:
            new_lines.append(line)
    text = '\n'.join(new_lines)

text = text.replace('class MYADDON_OT_ai_generate_enemy_plan(bpy.types.Operator):', level_gen_code + '\nclass MYADDON_OT_ai_generate_enemy_plan(bpy.types.Operator):')

if 'MYADDON_OT_ai_generate_level_obstacles' not in text.split('classes = (')[1]:
    text = text.replace('    MYADDON_OT_ai_generate_enemy_plan,\n', '    MYADDON_OT_ai_generate_level_obstacles,\n    MYADDON_OT_ai_generate_enemy_plan,\n')

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)
