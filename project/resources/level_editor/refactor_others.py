import re

filepath = 'c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/project/resources/level_editor/operators.py'
with open(filepath, 'r', encoding='utf-8') as f:
    text = f.read()

# Update MYADDON_OT_ai_enemy_chat
chat_pattern = re.compile(r"try:\s*plan_data = _request_gemini_enemy_plan\(\s*context,\s*scene,\s*count,\s*seed,\s*history,\s*wave_delay,\s*center,\s*extents,\s*player,\s*baseline_plan=builtin_plan\s*\)\s*if plan_data:\s*blueprints = _sanitize_enemy_plan_data\(plan_data, count, center, extents, player\)\s*gemini_used = True\s*", re.DOTALL)

def chat_replace(m):
    return """try:
                # We need to call _generate_gemini_with_retry here instead
                generated_objects, generated_enemies, blueprints, plan_data = _generate_gemini_with_retry(
                    self, context, scene, count, seed, history, None, None, wave_delay, center, extents, player, builtin_plan, None
                )
                if plan_data:
                    gemini_used = True
                    """

text = chat_pattern.sub(chat_replace, text)


# Update MYADDON_OT_ai_enemy_partial_regen
# Wait, MYADDON_OT_ai_enemy_partial_regen has:
# plan_data = _request_gemini_enemy_plan(context, scene, count, seed, history_msgs, wave_delay, center, extents, player, baseline_plan=builtin_plan)
partial_pattern = re.compile(r"plan_data = _request_gemini_enemy_plan\(\s*context,\s*scene,\s*count,\s*seed,\s*style,\s*prompt_override,\s*wave_delay,\s*center,\s*extents,\s*player,\s*\)\s*blueprints = _sanitize_enemy_plan_data\(plan_data, count, center, extents, player\)", re.DOTALL)

def partial_replace(m):
    return """generated_objects, generated_enemies, blueprints, plan_data = _generate_gemini_with_retry(
                        self, context, scene, count, seed, None, style, prompt_override, wave_delay, center, extents, player, None, collection
                    )"""

text = partial_pattern.sub(partial_replace, text)

# Wait, the partial regen operator passes `collection` but does it actually spawn? 
# In MYADDON_OT_ai_enemy_partial_regen it usually does:
# generated_objects, generated_enemies = _create_ai_enemy_objects_from_blueprints(...)
# Let's just fix MYADDON_OT_ai_enemy_partial_regen more carefully.

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(text)
print("Updated chat and partial regen operators.")
