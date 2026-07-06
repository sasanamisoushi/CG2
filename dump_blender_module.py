import sys
import inspect
import bpy

# Try to find the loaded module in sys.modules
mod_name = "project.resources.level_editor.operators"
mod = sys.modules.get(mod_name)
if not mod:
    # try full name
    for k in sys.modules.keys():
        if "operators" in k and "level_editor" in k:
            mod = sys.modules[k]
            break

if mod:
    try:
        source = inspect.getsource(mod)
        with open("recovered_operators.py", "w", encoding="utf-8") as f:
            f.write(source)
        print("Recovered from memory!")
    except Exception as e:
        print("Could not get source:", e)
else:
    print("Module not found in memory.")
