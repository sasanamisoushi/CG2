import sys

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Update MYADDON_OT_ai_generate_level_obstacles
old_generate = """        style = plan["style"]
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
        else: # ARENA or default
            base_count = int(10 * density)
            for i in range(base_count):
                angle = rng.uniform(0, math.pi * 2)
                radius = rng.uniform(extents.x * 0.3, extents.x * 0.9)
                lx = center.x + math.cos(angle) * radius
                ly = center.y + math.sin(angle) * radius
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((2.0, 2.0, rng.uniform(3.0, 8.0)))
                })"""

new_generate = """        style = plan["style"]
        shape = plan["shape"]
        base_count = getattr(scene, "myaddon_ai_level_count", 16)
        
        obstacles_to_create = []
        
        if style == "CITY":
            cols = max(2, int(math.sqrt(base_count)))
            rows = max(2, int(base_count / cols))
            spacing_x = (extents.x * 2 * 0.8) / cols
            spacing_y = (extents.y * 2 * 0.8) / rows
            
            for r in range(rows):
                for c in range(cols):
                    if rng.random() > 0.1:
                        lx = center.x - extents.x*0.8 + c * spacing_x + rng.uniform(-2, 2)
                        ly = center.y - extents.y*0.8 + r * spacing_y + rng.uniform(-2, 2)
                        obstacles_to_create.append({
                            "loc": Vector((lx, ly, 0)),
                            "scale": Vector((rng.uniform(1.0, 3.0), rng.uniform(1.0, 3.0), rng.uniform(4.0, 12.0)))
                        })
        elif style == "DEFENSE_LINE":
            spacing = (extents.x * 2 * 0.9) / max(1, base_count)
            for i in range(base_count):
                lx = center.x - extents.x*0.9 + i * spacing
                ly = center.y + extents.y * rng.uniform(0.1, 0.3)
                obstacles_to_create.append({
                    "loc": Vector((lx, ly + rng.uniform(-1, 1), 0)),
                    "scale": Vector((rng.uniform(2.0, 6.0), rng.uniform(1.0, 2.0), rng.uniform(2.0, 4.0)))
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
                    "scale": Vector((sx, sy, rng.uniform(3.0, 6.0)))
                })
        else: # ARENA or default
            for i in range(base_count):
                angle = rng.uniform(0, math.pi * 2)
                radius = rng.uniform(extents.x * 0.3, extents.x * 0.9)
                lx = center.x + math.cos(angle) * radius
                ly = center.y + math.sin(angle) * radius
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((2.0, 2.0, rng.uniform(3.0, 8.0)))
                })
        
        # Ensure all obstacles are clamped to bounds with margin
        for obs in obstacles_to_create:
            margin = Vector((obs["scale"].x, obs["scale"].y, 0.0))
            obs["loc"] = _clamp_point_to_box(obs["loc"], center, extents, margin)
"""

text = text.replace(old_generate, new_generate)

# 2. Add MYADDON_OT_randomize_ai_level_seed & MYADDON_OT_ai_edit_selected_obstacle
new_classes_code = """class MYADDON_OT_randomize_ai_level_seed(bpy.types.Operator):
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
        
        selected_obstacles = [obj for obj in context.selected_objects if getattr(obj, "game_obj_type", "") == 'OBSTACLE' or obj.name.startswith("LevelObstacle")]
        
        if not selected_obstacles:
            self.report({'ERROR'}, "再生成したい障害物を選択してください")
            return {'CANCELLED'}

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
        
        plan = _parse_ai_level_prompt(self.level_prompt)
        rng = random.Random(self.seed)
        shape = plan["shape"]
        
        col_name = "AI_Obstacles"
        obs_col = bpy.data.collections.get(col_name)
        if not obs_col:
            obs_col = bpy.data.collections.new(col_name)
            scene.collection.children.link(obs_col)

        updated_count = 0
        for obs_obj in selected_obstacles:
            spawn_loc = obs_obj.location.copy()
            
            # Remove old obstacle
            bpy.data.objects.remove(obs_obj, do_unlink=True)
            
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
            
            if new_obj.location.z < extents.z:
                new_obj.location.z = extents.z
            
            new_obj.name = f"LevelObstacle_Regen_{rng.randint(1000,9999)}"
            new_obj["game_obj_type"] = 'OBSTACLE'
            new_obj["myaddon_ai_level_prompt"] = self.level_prompt
            
            if new_obj.name not in obs_col.objects:
                obs_col.objects.link(new_obj)
            if new_obj.name in scene.collection.objects:
                scene.collection.objects.unlink(new_obj)
                
            updated_count += 1
            
        self.report({'INFO'}, f"{updated_count}個の障害物を個別に再生成しました")
        return {'FINISHED'}

"""

# Insert new classes before "classes = ("
classes_idx = text.rfind("classes = (")
text = text[:classes_idx] + new_classes_code + "\n" + text[classes_idx:]

# 3. Add to classes tuple
old_classes_tuple = """    MYADDON_OT_ai_generate_level_obstacles,
    MYADDON_OT_randomize_ai_seed,"""
new_classes_tuple = """    MYADDON_OT_ai_generate_level_obstacles,
    MYADDON_OT_randomize_ai_level_seed,
    MYADDON_OT_ai_edit_selected_obstacle,
    MYADDON_OT_randomize_ai_seed,"""

text = text.replace(old_classes_tuple, new_classes_tuple)

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)

print("SUCCESS")
