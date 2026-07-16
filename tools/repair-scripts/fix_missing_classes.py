import sys

op_file = r"project\resources\level_editor\operators.py"
with open(op_file, "r", encoding="utf-8") as f:
    text = f.read()

# The missing classes
new_classes_code = """class MYADDON_OT_randomize_ai_seed(bpy.types.Operator):
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
        self.motion_prompt = getattr(scene, "myaddon_ai_enemy_motion_prompt", "")
        self.seed = getattr(scene, "myaddon_ai_enemy_seed", 1)
        return context.window_manager.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "motion_prompt")
        layout.prop(self, "seed")
        layout.label(text="LLM有効時はこの位置・プロンプトで1体分のみ再生成します", icon='INFO')

    def execute(self, context):
        scene = context.scene
        selected_paths = [obj for obj in context.selected_objects if _is_enemy_path_object(obj)]
        if not selected_paths:
            self.report({'ERROR'}, "再生成したいEnemyPathを選択してください")
            return {'CANCELLED'}

        center, extents = _stage_bounds_box(scene)
        player = _find_player_spawn_position(scene, center, extents)
        boss = _find_boss_position(scene, center, extents)
        
        style = getattr(scene, "myaddon_ai_enemy_style", 'BALANCED')
        motion = _parse_ai_motion_prompt(self.motion_prompt, style)
        rng = random.Random(self.seed)
        provider = getattr(scene, "myaddon_ai_enemy_provider", 'BUILTIN')
        wave_delay = max(0, int(getattr(scene, "myaddon_ai_enemy_wave_delay", 90)))
        
        updated_count = 0
        for path_obj in selected_paths:
            path_id = getattr(path_obj, "enemy_path_id", path_obj.name)
            spawn = None
            side = 1.0
            for obj in scene.objects:
                if getattr(obj, "game_obj_type", "NONE") == "ENEMY" and getattr(obj, "enemy_path_id", "") == path_id:
                    spawn = obj.location.copy()
                    if spawn.x < player.x: side = -1.0
                    break
            
            if not spawn:
                spawn = path_obj.location.copy()

            if provider == 'GEMINI':
                try:
                    prompt_override = f"IMPORTANT: Partial regeneration. Enemy count must be exactly 1. The enemy MUST spawn exactly at {{'x': {spawn.x}, 'y': {spawn.y}, 'z': {spawn.z}}}. Ignore any global spacing rules and just generate a path starting at that point based on this request: {self.motion_prompt}"
                    
                    plan_data = _request_gemini_enemy_plan(
                        context,
                        scene,
                        1,
                        self.seed,
                        style,
                        prompt_override,
                        wave_delay,
                        center,
                        extents,
                        player,
                        boss,
                    )
                    blueprints = _sanitize_enemy_plan_data(plan_data, 1, center, extents, player)
                    if blueprints:
                        points = blueprints[0]["path"]
                        loop = blueprints[0]["loop"]
                        speed = blueprints[0]["speed"]
                        handle_type = blueprints[0]["handle_type"]
                    else:
                        points = []
                        loop = False
                        speed = 0.05
                        handle_type = 'AUTO'
                except Exception as exc:
                    self.report({'ERROR'}, f"Gemini再生成に失敗しました: {exc}")
                    continue
            else:
                points = _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, 0, rng, __import__('mathutils').Vector((0,0,0)))
                loop = motion["loop"] if motion.get("loop") is not None else style == 'PATROL'
                speed = 0.050 * motion.get("speed_multiplier", 1.0)
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
                    point.co = co
                    point.handle_left_type = handle_type
                    point.handle_right_type = handle_type
                
                path_obj.enemy_path_loop = loop
                path_obj.enemy_path_speed = speed
                path_obj["myaddon_ai_motion_prompt"] = self.motion_prompt
                updated_count += 1
                
        self.report({'INFO'}, f"{updated_count}個のパスを個別に再生成しました")
        return {'FINISHED'}


"""

if "class MYADDON_OT_randomize_ai_seed" not in text:
    text = text.replace("classes = (", new_classes_code + "classes = (")
    with open(op_file, "w", encoding="utf-8") as f:
        f.write(text)
    print("Added missing classes successfully!")
else:
    print("Classes already exist!")
