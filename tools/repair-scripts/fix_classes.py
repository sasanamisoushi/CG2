import sys

op_file = r"project\resources\level_editor\operators.py"
with open(op_file, "r", encoding="utf-8") as f:
    text = f.read()

old_classes = """classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
    MYADDON_OT_validate_scene,
    MYADDON_OT_playtest_game,
    MYADDON_OT_apply_active_properties_to_selection,
    MYADDON_OT_ai_generate_level_obstacles,
    MYADDON_OT_ai_generate_enemy_plan,
    MYADDON_OT_create_enemy_path,
    MYADDON_OT_assign_selected_enemy_path,
    MYADDON_OT_assign_selected_reinforcement_trigger,
    MYADDON_OT_create_stage_bounds,
    MYADDON_OT_create_spawn_point,
)"""

new_classes = """classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
    MYADDON_OT_validate_scene,
    MYADDON_OT_playtest_game,
    MYADDON_OT_apply_active_properties_to_selection,
    MYADDON_OT_ai_generate_level_obstacles,
    MYADDON_OT_randomize_ai_level_seed,
    MYADDON_OT_ai_edit_selected_obstacle,
    MYADDON_OT_randomize_ai_seed,
    MYADDON_OT_ai_generate_enemy_plan,
    MYADDON_OT_ai_edit_selected_enemy_path,
    MYADDON_OT_create_enemy_path,
    MYADDON_OT_assign_selected_enemy_path,
    MYADDON_OT_assign_selected_reinforcement_trigger,
    MYADDON_OT_create_stage_bounds,
    MYADDON_OT_create_spawn_point,
)"""

if old_classes in text:
    text = text.replace(old_classes, new_classes)
    with open(op_file, "w", encoding="utf-8") as f:
        f.write(text)
    print("Fixed classes tuple!")
else:
    print("Could not find old_classes exactly as written.")
