import sys
import os

op_file = "c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/project/resources/level_editor/operators.py"
with open(op_file, 'r', encoding='utf-8') as f:
    op_code = f.read()

new_class = '''
class MYADDON_OT_ai_edit_selected_enemy_path(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_edit_selected_enemy_path"
    bl_label = "選択したパスをAIで再生成"
    bl_description = "選択中のEnemyPathを、現在のプロンプトを使って再生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        scene = context.scene
        selected_paths = [obj for obj in context.selected_objects if _is_enemy_path_object(obj)]
        if not selected_paths:
            self.report({'ERROR'}, "再生成したいEnemyPathを選択してください")
            return {'CANCELLED'}

        center, extents = _stage_bounds_box(scene)
        player = _find_player_spawn_position(scene, center, extents)
        
        style = getattr(scene, "myaddon_ai_enemy_style", 'BALANCED')
        motion_prompt = getattr(scene, "myaddon_ai_enemy_motion_prompt", "")
        motion = _parse_ai_motion_prompt(motion_prompt, style)
        rng = random.Random(int(getattr(scene, "myaddon_ai_enemy_seed", 1)))
        
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
            
            points = _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, 0, rng)
            if path_obj.type == 'CURVE' and points:
                curve = path_obj.data
                spline = curve.splines[0] if curve.splines else curve.splines.new('BEZIER')
                if len(spline.bezier_points) < len(points):
                    spline.bezier_points.add(len(points) - len(spline.bezier_points))
                elif len(spline.bezier_points) > len(points):
                    curve.splines.remove(spline)
                    spline = curve.splines.new('BEZIER')
                    spline.bezier_points.add(len(points) - 1)
                
                loop = motion["loop"] if motion.get("loop") is not None else style == 'PATROL'
                spline.use_cyclic_u = loop
                
                handle_type = 'VECTOR' if motion.get("pattern") == "EDGE_ORBIT" and motion.get("respect_bounds") else 'AUTO'
                for point, co in zip(spline.bezier_points, points):
                    point.co = co
                    point.handle_left_type = handle_type
                    point.handle_right_type = handle_type
                
                speed = 0.050 * motion.get("speed_multiplier", 1.0)
                path_obj.enemy_path_loop = loop
                path_obj.enemy_path_speed = speed
                path_obj["myaddon_ai_motion_prompt"] = motion_prompt
                updated_count += 1
                
        self.report({'INFO'}, f"{updated_count}個のパスをAIで再生成しました")
        return {'FINISHED'}

classes = (
'''

if "MYADDON_OT_ai_edit_selected_enemy_path" not in op_code:
    op_code = op_code.replace("classes = (", new_class)
    op_code = op_code.replace("MYADDON_OT_ai_generate_enemy_plan,", "MYADDON_OT_ai_generate_enemy_plan,\n    MYADDON_OT_ai_edit_selected_enemy_path,")
    with open(op_file, 'w', encoding='utf-8') as f:
        f.write(op_code)

ui_file = "c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/project/resources/level_editor/ui.py"
with open(ui_file, 'r', encoding='utf-8') as f:
    ui_code = f.read()

if "MYADDON_OT_ai_edit_selected_enemy_path" not in ui_code:
    ui_code = ui_code.replace(
        "ai_box.operator(operators.MYADDON_OT_ai_generate_enemy_plan.bl_idname, text=\"AIで敵プラン生成\", icon='MOD_PARTICLES')",
        "ai_box.operator(operators.MYADDON_OT_ai_generate_enemy_plan.bl_idname, text=\"AIで敵プラン生成\", icon='MOD_PARTICLES')\n        ai_box.operator(operators.MYADDON_OT_ai_edit_selected_enemy_path.bl_idname, text=\"選択したパスだけAIで再生成\", icon='GREASEPENCIL')"
    )
    with open(ui_file, 'w', encoding='utf-8') as f:
        f.write(ui_code)
