import re
import os

filepath = 'c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/project/resources/level_editor/operators.py'
with open(filepath, 'r', encoding='utf-8') as f:
    text = f.read()

# 1. We need to remove the first set of duplicated AI functions to fix the bug.
pattern1 = r"(def _ai_enemy_plan_schema\(\):.*?)(def _ai_enemy_plan_schema\(\):)"
match = re.search(pattern1, text, re.DOTALL)
if match:
    # Remove the first block
    text = text[:match.start()] + match.group(2) + text[match.end():]
    print("Removed duplicated AI functions.")

# 2. Add JSON decode error handling and self-correction loop in the operators.
helper = """
def _generate_gemini_with_retry(self, context, scene, count, seed, history, style, motion_prompt, wave_delay, center, extents, player, baseline_plan, collection):
    import json
    import bpy
    from . import validation

    max_retries = 3
    current_history = list(history) if history else []

    if not current_history and style and motion_prompt:
        from . import properties
        msg = properties.EnemyChatMsg()
        msg.role = "USER"
        msg.content = f"Style: {style}, Request: {motion_prompt}"
        current_history = [msg]

    for attempt in range(max_retries):
        if attempt > 0:
            self.report({'INFO'}, f"AI修正ループ: {attempt}/{max_retries - 1}回目...")
            
        try:
            # Generate JSON
            plan_data = _request_gemini_enemy_plan(
                context, scene, count, seed, current_history, wave_delay, center, extents, player, baseline_plan
            )
            # Parse blueprints
            blueprints = _sanitize_enemy_plan_data(plan_data, count, center, extents, player)
        except json.JSONDecodeError as e:
            self.report({'WARNING'}, "JSONのフォーマットが崩れています。リトライします。")
            from . import properties
            err_msg = properties.EnemyChatMsg()
            err_msg.role = "USER"
            err_msg.content = "JSONのフォーマットが崩れています。謝罪や挨拶などの文章は一切不要です。修正した正しいJSONデータのみを出力してください。"
            current_history.append(err_msg)
            continue
        except Exception as e:
            raise e

        # Spawn
        generated_objects, generated_enemies = _create_ai_enemy_objects_from_blueprints(
            scene, collection, blueprints, motion_prompt, player, "GEMINI"
        )
        
        # Validate
        errors, warnings = validation.validate_and_store(scene)
        
        if not errors:
            return generated_objects, generated_enemies, blueprints, plan_data

        if attempt < max_retries - 1:
            # Delete spawned objects
            for obj in generated_objects:
                bpy.data.objects.remove(obj, do_unlink=True)
                
            error_text = "\\n".join(errors)
            from . import properties
            err_msg = properties.EnemyChatMsg()
            err_msg.role = "USER"
            err_msg.content = f"あなたが生成したデータは以下のエラーになりました。\\n{error_text}\\nこのエラーを回避するように座標を修正して再出力してください。謝罪や挨拶などの文章は一切不要です。修正したJSONデータのみを出力してください。"
            current_history.append(err_msg)
        else:
            # Reached max retries
            self.report({'WARNING'}, "AI修正ループが上限に達しました。エラー状態のまま配置します。")
            error_col = bpy.data.collections.get("Error_Enemies")
            if not error_col:
                error_col = bpy.data.collections.new("Error_Enemies")
                scene.collection.children.link(error_col)
            for obj in generated_objects:
                for col in obj.users_collection:
                    col.objects.unlink(obj)
                error_col.objects.link(obj)
            return generated_objects, generated_enemies, blueprints, plan_data

    return [], [], [], None
"""

text = text.replace("class MYADDON_OT_ai_generate_enemy_plan(", helper + "\nclass MYADDON_OT_ai_generate_enemy_plan(")

# MYADDON_OT_ai_generate_enemy_plan modification
gen_pattern = re.compile(r"if provider == 'GEMINI':\s*try:\s*plan_data = _request_gemini_enemy_plan\(.*?\)\s*blueprints = _sanitize_gemini_enemy_plan\(.*?\)\s*if getattr\(scene, \"myaddon_ai_enemy_clear_existing\", True\):\s*_delete_ai_generated_objects\(scene\)\s*generated_objects, generated_enemies = _create_ai_enemy_objects_from_blueprints\(.*?\)\s*", re.DOTALL)

def gen_replace(m):
    return """if provider == 'GEMINI':
            try:
                if getattr(scene, "myaddon_ai_enemy_clear_existing", True):
                    _delete_ai_generated_objects(scene)
                    
                generated_objects, generated_enemies, blueprints, plan_data = _generate_gemini_with_retry(
                    self, context, scene, count, seed, history, style, motion_prompt, wave_delay, center, extents, player, None, collection
                )
"""

text = gen_pattern.sub(gen_replace, text)

# For chat operator (wait, does the chat operator exist in enemies? Let's check where _request_gemini_enemy_plan is called)
chat_pattern = re.compile(r"plan_data = _request_gemini_enemy_plan\(\s*context,\s*scene,\s*count,\s*seed,\s*history,\s*wave_delay,\s*center,\s*extents,\s*player,\s*baseline_plan=builtin_plan\s*\)\s*if plan_data:\s*blueprints = _sanitize_enemy_plan_data\(plan_data, count, center, extents, player\)\s*gemini_used = True", re.DOTALL)

def chat_replace(m):
    return """generated_objects, generated_enemies, blueprints, plan_data = _generate_gemini_with_retry(
                    self, context, scene, count, seed, history, None, None, wave_delay, center, extents, player, builtin_plan, None
                )
                if plan_data:
                    gemini_used = True"""

text = chat_pattern.sub(chat_replace, text)

# For partial regen operator
partial_pattern = re.compile(r"plan_data = _request_gemini_enemy_plan\(\s*context,\s*scene,\s*count,\s*seed,\s*style,\s*prompt_override,\s*wave_delay,\s*center,\s*extents,\s*player,\s*\)\s*blueprints = _sanitize_enemy_plan_data\(plan_data, count, center, extents, player\)", re.DOTALL)

def partial_replace(m):
    return """generated_objects, generated_enemies, blueprints, plan_data = _generate_gemini_with_retry(
                        self, context, scene, count, seed, None, style, prompt_override, wave_delay, center, extents, player, None, collection
                    )"""

text = partial_pattern.sub(partial_replace, text)

text = text.replace("_sanitize_gemini_enemy_plan", "_sanitize_enemy_plan_data")

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(text)
print("Refactoring complete.")
