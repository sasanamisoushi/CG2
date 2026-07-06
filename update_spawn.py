import sys

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

old_execute = '''        for pair_index in range((count + 1) // 2):
            for side in (1.0, -1.0):
                if spawned_count >= count:
                    break

                spawn = center + right * (extents.x * 0.4 * side) + forward * (extents.y * 0.45)
                spawn.z += rng.uniform(-1.0, 1.0)
                spawn = _clamp_point_to_box(spawn, center, extents, Vector((1.0, 1.0, 0.8)))'''

new_execute = '''        for pair_index in range((count + 1) // 2):
            for side in (1.0, -1.0):
                if spawned_count >= count:
                    break

                spawn = center + right * (extents.x * 0.4 * side) + forward * (extents.y * 0.45)
                spawn.z += rng.uniform(-1.0, 1.0)
                spawn = _clamp_point_to_box(spawn, center, extents, Vector((1.0, 1.0, 0.8)))
                spawn = _push_out_of_obstacles(spawn, scene)'''

text = text.replace(old_execute, new_execute)

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)
