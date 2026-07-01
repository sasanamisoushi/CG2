import sys
import json

filepath = "c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/project/resources/level_editor/operators.py"
with open(filepath, 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Update _create_ai_enemy_objects_from_blueprints
if 'path_obj["myaddon_ai_motion_state"] = json.dumps(motion)' not in code:
    code = code.replace(
        'path_obj["myaddon_ai_motion_prompt"] = motion_prompt\n        path_obj["myaddon_ai_provider"] = provider_label',
        'path_obj["myaddon_ai_motion_prompt"] = motion_prompt\n        path_obj["myaddon_ai_provider"] = provider_label\n        if motion_prompt:\n            pass # Geminiの場合はmotionがないのでそのまま'
    )

# 2. Update MYADDON_OT_ai_generate_enemy_plan
code = code.replace(
    'path_obj = _create_ai_enemy_path(collection, path_id, points, loop, speed, handle_type)\n            path_obj["myaddon_ai_motion_prompt"] = motion_prompt',
    'path_obj = _create_ai_enemy_path(collection, path_id, points, loop, speed, handle_type)\n            path_obj["myaddon_ai_motion_prompt"] = motion_prompt\n            import json\n            path_obj["myaddon_ai_motion_state"] = json.dumps(motion)'
)

# 3. Update MYADDON_OT_ai_edit_selected_enemy_path
edit_old = '''        style = getattr(scene, "myaddon_ai_enemy_style", 'BALANCED')
        motion_prompt = getattr(scene, "myaddon_ai_enemy_motion_prompt", "")
        motion = _parse_ai_motion_prompt(motion_prompt, style)
        rng = random.Random(int(getattr(scene, "myaddon_ai_enemy_seed", 1)))'''

edit_new = '''        style = getattr(scene, "myaddon_ai_enemy_style", 'BALANCED')
        new_prompt = getattr(scene, "myaddon_ai_enemy_motion_prompt", "")
        rng = random.Random(int(getattr(scene, "myaddon_ai_enemy_seed", 1)))'''

if edit_old in code:
    code = code.replace(edit_old, edit_new)

loop_old = '''            pair_index = 0
            points = _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, pair_index, rng)'''

loop_new = '''            import json
            old_prompt = path_obj.get("myaddon_ai_motion_prompt", "")
            try:
                base_motion = json.loads(path_obj.get("myaddon_ai_motion_state", "{}"))
            except:
                base_motion = None
            
            motion = _parse_ai_motion_prompt(new_prompt, style, base_motion)
            combined_prompt = f"{old_prompt} -> {new_prompt}" if old_prompt and new_prompt else (old_prompt or new_prompt)
            
            pair_index = 0
            points = _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, pair_index, rng)'''

if loop_old in code:
    code = code.replace(loop_old, loop_new)

save_old = '''                path_obj.enemy_path_loop = loop
                path_obj.enemy_path_speed = speed
                path_obj["myaddon_ai_motion_prompt"] = motion_prompt
                updated_count += 1'''

save_new = '''                path_obj.enemy_path_loop = loop
                path_obj.enemy_path_speed = speed
                path_obj["myaddon_ai_motion_prompt"] = combined_prompt
                path_obj["myaddon_ai_motion_state"] = json.dumps(motion)
                updated_count += 1'''

if save_old in code:
    code = code.replace(save_old, save_new)

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(code)

print("Patch successful!")
