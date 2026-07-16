import re

filepath = 'c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/project/resources/level_editor/operators.py'
with open(filepath, 'r', encoding='utf-8') as f:
    text = f.read()

# Make _request_gemini_enemy_plan NOT swallow exceptions, so our retry loop can catch JSONDecodeError
# It currently has:
#    except Exception as exc:
#        print(f"Gemini Enemy Plan error: {exc}")
#        return None
# Let's replace it with:
pattern = re.compile(r"except Exception as exc:\s*print\(f\"Gemini Enemy Plan error: \{exc\}\"\)\s*return None", re.MULTILINE)
text = pattern.sub("except Exception as exc:\n        print(f\"Gemini Enemy Plan error: {exc}\")\n        raise exc", text)

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(text)
print("Updated Exception handling in operators.")
