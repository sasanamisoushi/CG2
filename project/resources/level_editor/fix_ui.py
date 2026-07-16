import os

dir_path = 'c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/project/resources/level_editor'

# 1. Fix encoding for all python files (add BOM)
for filename in os.listdir(dir_path):
    if filename.endswith(".py"):
        filepath = os.path.join(dir_path, filename)
        
        # Read the file. Try utf-8-sig first, then utf-8, then shift_jis
        content = None
        for enc in ['utf-8-sig', 'utf-8', 'shift_jis']:
            try:
                with open(filepath, 'r', encoding=enc) as f:
                    content = f.read()
                break
            except Exception:
                pass
        
        if content is not None:
            # Write it back as utf-8-sig (UTF-8 with BOM)
            with open(filepath, 'w', encoding='utf-8-sig') as f:
                f.write(content)

# 2. Fix the menu duplication bug in __init__.py
init_path = os.path.join(dir_path, "__init__.py")
with open(init_path, 'r', encoding='utf-8-sig') as f:
    text = f.read()

# Replace the module deletion block
old_block = """for mod in ["properties", "validation", "operators", "ui", "draw", "network"]:
    if mod in sys.modules:
        del sys.modules[mod]"""

new_block = """# 安全に古いモジュールの登録を解除してからキャッシュを削除する
for mod in ["network", "draw", "ui", "operators", "validation", "properties"]:
    if mod in sys.modules:
        try:
            sys.modules[mod].unregister()
        except Exception:
            pass
        del sys.modules[mod]"""

if old_block in text:
    text = text.replace(old_block, new_block)
else:
    print("Could not find old block in __init__.py")

with open(init_path, 'w', encoding='utf-8-sig') as f:
    f.write(text)

print("Encoding and reload fixes applied.")
