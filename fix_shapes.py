import sys

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

old_build = '''    if style == 'SWARM':
        lateral *= 0.45
        raw_points = [
            spawn,
            player + forward * min(extents.y * 0.22, 5.0) + right * lateral + Vector((0.0, 0.0, z_swing)),
            player - right * lateral + Vector((0.0, 0.0, -z_swing)),
            spawn - forward * min(extents.y * 0.22, 5.0),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if style == 'AMBUSH':
        lateral *= 1.15
        raw_points = [
            spawn,
            center + right * lateral + Vector((0.0, 0.0, z_swing)),
            player - right * (lateral * 0.65) + Vector((0.0, 0.0, -z_swing)),
            center - forward * min(extents.y * 0.38, 9.0) - right * (lateral * 0.35),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    raw_points = [
        spawn,
        center + right * lateral + Vector((0.0, 0.0, z_swing)),
        player + forward * min(extents.y * 0.2, 6.0) - right * (lateral * 0.45),
        center - forward * min(extents.y * 0.35, 8.0) - right * (lateral * 0.25),
    ]'''

new_build = '''    if style == 'SWARM':
        lateral *= 0.45
        raw_points = [
            spawn,
            player + forward * min(extents.y * 0.22, 5.0) + right * lateral + Vector((0.0, 0.0, z_swing)),
            player - forward * min(extents.y * 0.1, 2.0) - right * lateral + Vector((0.0, 0.0, -z_swing)),
            spawn + right * (lateral * 0.5) + forward * min(extents.y * 0.1, 2.0),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    if style == 'AMBUSH':
        lateral *= 1.15
        raw_points = [
            spawn,
            center + right * lateral + Vector((0.0, 0.0, z_swing)),
            player - right * (lateral * 0.65) + Vector((0.0, 0.0, -z_swing)),
            spawn + right * (lateral * 0.5) + forward * min(extents.y * 0.15, 3.0),
        ]
        raw_points = _apply_vertical_motion(raw_points, motion, extents)
        return [_clamp_point_to_box(point, center, extents, margin) for point in raw_points]

    raw_points = [
        spawn,
        center + right * lateral + Vector((0.0, 0.0, z_swing)),
        player + forward * min(extents.y * 0.2, 6.0) - right * (lateral * 0.45),
        spawn + right * (lateral * 0.5) + forward * min(extents.y * 0.15, 3.0),
    ]'''

text = text.replace(old_build, new_build)

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)
