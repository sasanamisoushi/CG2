import sys
import re

op_path = r"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py"
with open(op_path, "r", encoding="utf-8") as f:
    text = f.read()

# 1. Modify _parse_ai_enemy_prompt signature and inner logic
start_idx = text.find("def _parse_ai_enemy_prompt(prompt_text):")
end_idx = text.find("def _build_edge_orbit_points(", start_idx)

if start_idx != -1 and end_idx != -1:
    orig_func = text[start_idx:end_idx]
    
    new_func = orig_func.replace(
        "def _parse_ai_enemy_prompt(prompt_text):\n    normalized = str(prompt_text or \"\").casefold()\n",
        "def _parse_ai_enemy_prompt(prompt_text):\n    normalized = str(prompt_text or \"\").casefold()\n    matched_any = False\n\n    def check(*kws):\n        nonlocal matched_any\n        if _prompt_has(normalized, *kws):\n            matched_any = True\n            return True\n        return False\n"
    )
    new_func = new_func.replace("_prompt_has(normalized,", "check(")
    new_func = new_func.replace("return style, motion", "return style, motion, matched_any")
    
    # Update callers in operators.py
    text = text[:start_idx] + new_func + text[end_idx:]
    text = text.replace("style, motion = _parse_ai_enemy_prompt(full_text)", "style, motion, _ = _parse_ai_enemy_prompt(full_text)")
    text = text.replace("base_style, _ = _parse_ai_enemy_prompt(full_text)", "base_style, _, _ = _parse_ai_enemy_prompt(full_text)")
    text = text.replace("style, motion = _parse_ai_enemy_prompt(self.motion_prompt)", "style, motion, _ = _parse_ai_enemy_prompt(self.motion_prompt)")

    with open(op_path, "w", encoding="utf-8") as f:
        f.write(text)
    print("operators.py patched")
else:
    print("failed to find function bounds in operators.py")

gemini_path = r"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\async_gemini.py"
with open(gemini_path, "r", encoding="utf-8") as f:
    g_text = f.read()

old_fallback = """
        if provider != 'OLLAMA':
            return bpy.ops.myaddon.myaddon_ot_ai_generate_enemy_plan()
"""
new_fallback = """
        if provider != 'OLLAMA':
            fallback_enabled = getattr(scene, "myaddon_ai_enemy_ollama_fallback", True)
            if fallback_enabled:
                from .operators import _parse_ai_enemy_prompt
                # re-compute full_text for fallback check
                full_text = " ".join([m.content for m in history]) + " " + prompt
                _, _, matched_any = _parse_ai_enemy_prompt(full_text)
                if not matched_any and len(full_text.strip()) > 0:
                    provider = 'OLLAMA'
                else:
                    return bpy.ops.myaddon.myaddon_ot_ai_generate_enemy_plan()
            else:
                return bpy.ops.myaddon.myaddon_ot_ai_generate_enemy_plan()
"""

if old_fallback in g_text:
    g_text = g_text.replace(old_fallback, new_fallback)
    with open(gemini_path, "w", encoding="utf-8") as f:
        f.write(g_text)
    print("async_gemini.py patched")
else:
    print("failed to find old_fallback in async_gemini.py")
