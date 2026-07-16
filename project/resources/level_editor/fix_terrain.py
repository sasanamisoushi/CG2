import bpy
import sys

filepath = 'c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/project/resources/level_editor/stage.blend'

# Open the file
bpy.ops.wm.open_mainfile(filepath=filepath)

changed = False
for obj in bpy.data.objects:
    # If the object is the heavy terrain or has many polygons
    if "Terrain" in obj.name or len(obj.data.polygons) > 10000 if obj.type == 'MESH' else False:
        if obj.display_type != 'BOUNDS':
            obj.display_type = 'BOUNDS'
            changed = True
            print(f"Set {obj.name} to BOUNDS display type")

if changed:
    bpy.ops.wm.save_mainfile(filepath=filepath)
    print("File saved successfully.")
else:
    print("No changes needed.")
