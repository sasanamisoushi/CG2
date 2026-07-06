import sys

op_file = r"project\resources\level_editor\operators.py"
with open(op_file, "r", encoding="utf-8") as f:
    text = f.read()

# Fix getattr
text = text.replace('getattr(obj, "game_obj_type", "NONE")', 'obj.get("game_obj_type", "NONE")')
text = text.replace('getattr(obj, "game_obj_type", "")', 'obj.get("game_obj_type", "")')
text = text.replace('getattr(obj, "enemy_path_id", "None")', 'obj.get("enemy_path_id", "None")')
text = text.replace('getattr(obj, "enemy_path_id", "")', 'obj.get("enemy_path_id", "")')

# _parse_ai_level_prompt scale modifiers
old_parse_level = """    plan = {
        "style": "CITY",
        "shape": "CUBE",
        "density_multiplier": 1.0,
    }"""
new_parse_level = """    plan = {
        "style": "CITY",
        "shape": "CUBE",
        "density_multiplier": 1.0,
        "is_modifier": False,
        "scale_modifiers": __import__('mathutils').Vector((1.0, 1.0, 1.0))
    }

    if _prompt_has(normalized, "小さく", "小さい", "縮小", "small", "shrink"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((0.5, 0.5, 0.5))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "大きく", "大きい", "拡大", "big", "large", "huge"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((2.0, 2.0, 2.0))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "高く", "高い", "tall", "height"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((1.0, 1.0, 2.0))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "低く", "低い", "short"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((1.0, 1.0, 0.5))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "長く", "長い", "long"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((2.0, 1.0, 1.0))
        plan["is_modifier"] = True
    elif _prompt_has(normalized, "平たく", "平ら", "flat"):
        plan["scale_modifiers"] = __import__('mathutils').Vector((1.0, 1.0, 0.2))
        plan["is_modifier"] = True"""
text = text.replace(old_parse_level, new_parse_level)

# _build_ai_level_obstacles LevelObstacle name
old_build = """        obj = context.active_object
        obj.scale = scale
        obj["ai_generated_obstacle"] = True"""
new_build = """        obj = context.active_object
        obj.name = f"LevelObstacle_{1000 + len(created_objs)}"
        obj.scale = scale
        obj["ai_generated_obstacle"] = True"""
text = text.replace(old_build, new_build)

# MYADDON_OT_ai_edit_selected_obstacle scale update
old_edit = """        updated_count = 0
        for obs_obj in selected_obstacles:
            spawn_loc = obs_obj.location.copy()
            
            # Remove old obstacle
            bpy.data.objects.remove(obs_obj, do_unlink=True)"""
new_edit = """        updated_count = 0
        for obs_obj in selected_obstacles:
            if plan.get("is_modifier"):
                obs_obj.scale.x *= plan["scale_modifiers"].x
                obs_obj.scale.y *= plan["scale_modifiers"].y
                obs_obj.scale.z *= plan["scale_modifiers"].z
                
                # Enforce bounds
                margin = Vector((obs_obj.scale.x, obs_obj.scale.y, 0.0))
                obs_obj.location = _clamp_point_to_box(obs_obj.location, center, extents, margin)
                if obs_obj.location.z < extents.z:
                    obs_obj.location.z = extents.z
                
                obs_obj["myaddon_ai_level_prompt"] = self.level_prompt
                updated_count += 1
                continue
                
            spawn_loc = obs_obj.location.copy()
            
            # Remove old obstacle
            bpy.data.objects.remove(obs_obj, do_unlink=True)"""
text = text.replace(old_edit, new_edit)

with open(op_file, "w", encoding="utf-8") as f:
    f.write(text)
print("Fixes applied.")
