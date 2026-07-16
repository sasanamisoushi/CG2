import sys

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Rename _sanitize_gemini_enemy_plan to _sanitize_enemy_plan_data and complete it
old_sanitize = """def _sanitize_gemini_enemy_plan(plan_data, count, center, extents, player):
    import bpy
    scene = bpy.context.scene
    enemies = plan_data.get("enemies") if isinstance(plan_data, dict) else None
    if not isinstance(enemies, list) or not enemies:
        raise ValueError("Geminiから敵データが返ってきませんでした。")

    blueprints = []
    margin = Vector((1.0, 1.0, 0.8))
    for index, enemy in enumerate(enemies[:count]):
        if not isinstance(enemy, dict):
            continue

        raw_path = enemy.get("path", [])
        points = []
        if isinstance(raw_path, list):
            for raw_point in raw_path[:12]:
                point = _coerce_ai_point(raw_point)
                if point is not None:
                    point = _clamp_point_to_box(point, center, extents, margin)
                    point = _push_out_of_obstacles(point, scene)
                    points.append(point)

        spawn = _coerce_ai_point(enemy.get("spawn"))
        if spawn is None and points:
            spawn = points[0]
        if spawn is None:
            continue
            
        spawn = _clamp_point_to_box(spawn, center, extents, margin)
        spawn = _push_out_of_obstacles(spawn, scene)

        if points:
            if (points[0] - spawn).length > 0.05:
                points.insert(0, spawn)
            else:
                points[0] = spawn
        else:
            points = [spawn]

        points = _sanitize_path_distances(points)
        if len(points) < 2 or all((point - spawn).length < 0.05 for point in points[1:]):
            points.append(_fallback_second_path_point(spawn, player, center, extents))

        enemy_type = str(enemy.get("enemy_type", "")).strip()
        if enemy_type not in {"VF1", "VF1-1"}:
            enemy_type = "VF1-1" if index % 3 == 2 else "VF1"
"""

new_sanitize = """def _sanitize_enemy_plan_data(plan_data, count, center, extents, player):
    import bpy
    scene = bpy.context.scene
    enemies = plan_data.get("enemies") if isinstance(plan_data, dict) else None
    if not isinstance(enemies, list) or not enemies:
        raise ValueError("AIから敵データが返ってきませんでした。")

    blueprints = []
    margin = Vector((1.0, 1.0, 0.8))
    for index, enemy in enumerate(enemies[:count]):
        if not isinstance(enemy, dict):
            continue

        raw_path = enemy.get("path", [])
        points = []
        if isinstance(raw_path, list):
            for raw_point in raw_path[:12]:
                point = _coerce_ai_point(raw_point)
                if point is not None:
                    point = _clamp_point_to_box(point, center, extents, margin)
                    point = _push_out_of_obstacles(point, scene)
                    points.append(point)

        spawn = _coerce_ai_point(enemy.get("spawn"))
        if spawn is None and points:
            spawn = points[0]
        if spawn is None:
            continue
            
        spawn = _clamp_point_to_box(spawn, center, extents, margin)
        spawn = _push_out_of_obstacles(spawn, scene)

        if points:
            if (points[0] - spawn).length > 0.05:
                points.insert(0, spawn)
            else:
                points[0] = spawn
        else:
            points = [spawn]

        points = _sanitize_path_distances(points)
        if len(points) < 2 or all((point - spawn).length < 0.05 for point in points[1:]):
            points.append(_fallback_second_path_point(spawn, player, center, extents))

        enemy_type = str(enemy.get("enemy_type", "")).strip()
        if enemy_type not in {"VF1", "VF1-1"}:
            enemy_type = "VF1-1" if index % 3 == 2 else "VF1"

        loop = bool(enemy.get("loop", False))
        speed = _clamp_float(enemy.get("speed", 0.05), 0.05, 0.015, 0.18)
        trigger_index = int(enemy.get("trigger_index", -1))
        delay_frames = int(enemy.get("delay_frames", 0))
        handle_type = str(enemy.get("handle_type", "AUTO")).strip()
        if handle_type not in {"AUTO", "VECTOR", "ALIGNED", "FREE"}:
            handle_type = "AUTO"

        blueprints.append({
            "spawn": spawn,
            "path": points,
            "enemy_type": enemy_type,
            "loop": loop,
            "speed": speed,
            "trigger_index": trigger_index,
            "delay_frames": delay_frames,
            "handle_type": handle_type,
        })
        
    return blueprints
"""
text = text.replace(old_sanitize, new_sanitize)

# 2. Rename _gemini_enemy_plan_schema to _ai_enemy_plan_schema and make it strictly typed
old_schema = """def _gemini_enemy_plan_schema():
    point_schema = {
        "type": "object",
        "properties": {
            "x": {"type": "number"},
            "y": {"type": "number"},
            "z": {"type": "number"},
        },
        "required": ["x", "y", "z"],
    }
    return {
        "type": "object",
        "properties": {
            "enemies": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "spawn": point_schema,
                        "path": {
                            "type": "array",
                            "items": point_schema,
                        },
                        "loop": {"type": "boolean"},
                        "speed": {"type": "number"},
                        "enemy_type": {
                            "type": "string",
                            "enum": ["VF1", "VF1-1"],
                        },
                        "trigger_index": {"type": "integer"},
                        "delay_frames": {"type": "integer"},
                        "handle_type": {
                            "type": "string",
                            "enum": ["AUTO", "VECTOR"],
                        },
                    },
                    "required": ["spawn", "path", "loop", "speed", "enemy_type"],
                },
            },
            "notes": {"type": "string"},
        },
        "required": ["enemies"],
    }"""

new_schema = """def _ai_enemy_plan_schema():
    point_schema = {
        "type": "object",
        "properties": {
            "x": {"type": "number"},
            "y": {"type": "number"},
            "z": {"type": "number"},
        },
        "required": ["x", "y", "z"],
    }
    return {
        "type": "object",
        "properties": {
            "enemies": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "spawn": point_schema,
                        "path": {
                            "type": "array",
                            "items": point_schema,
                        },
                        "loop": {"type": "boolean"},
                        "speed": {"type": "number"},
                        "enemy_type": {
                            "type": "string",
                            "enum": ["VF1", "VF1-1"],
                        },
                        "trigger_index": {"type": "integer"},
                        "delay_frames": {"type": "integer"},
                        "handle_type": {
                            "type": "string",
                            "enum": ["AUTO", "VECTOR", "ALIGNED", "FREE"],
                        },
                    },
                    "required": ["spawn", "path", "loop", "speed", "enemy_type", "trigger_index", "delay_frames", "handle_type"],
                },
            },
            "notes": {"type": "string"},
        },
        "required": ["enemies"],
    }"""
text = text.replace(old_schema, new_schema)

# 3. Update references
text = text.replace("_gemini_enemy_plan_schema()", "_ai_enemy_plan_schema()")
text = text.replace("blueprints = _sanitize_gemini_enemy_plan(plan_data", "blueprints = _sanitize_enemy_plan_data(plan_data")

# 4. Refactor BUILTIN loop in execute()
old_builtin = """        lane_count = max(1, (count + 1) // 2)
        formation_offsets = _build_ai_formation_offsets(count, extents, motion)
        speed_by_style = {
            'BALANCED': 0.050,
            'AMBUSH': 0.060,
            'SWARM': 0.072,
            'PATROL': 0.038,
        }
        generated_objects = []
        generated_enemies = []
        opener_count = min(count, 4 if style == 'SWARM' else 2)
        if motion.get("opener") == "ALL":
            opener_count = count
        elif motion.get("opener") == "ONE":
            opener_count = 1
        margin = Vector((2.0, 2.0, 1.2))
        if getattr(scene, "myaddon_ai_enemy_clear_existing", True):
            _delete_ai_generated_objects(scene)

        for index in range(count):
            pair_index = index // 2
            lane_t = 0.0 if lane_count <= 1 else pair_index / (lane_count - 1)
            side = -1.0 if index % 2 == 0 else 1.0
            z_span = min(extents.z * 0.32, 4.0)
            height = rng.uniform(-z_span, z_span)

            if style == 'AMBUSH':
                lateral = side * extents.x * rng.uniform(0.55, 0.82)
                depth = extents.y * rng.uniform(-0.05, 0.42)
            elif style == 'SWARM':
                swarm_step = min(2.2, extents.x * 0.12)
                lateral = (index - (count - 1) * 0.5) * swarm_step + rng.uniform(-0.7, 0.7)
                depth = extents.y * rng.uniform(0.18, 0.34)
            elif style == 'PATROL':
                lateral = side * extents.x * (0.28 + 0.38 * lane_t)
                depth = extents.y * (0.02 + 0.34 * lane_t)
            else:
                lateral = side * extents.x * (0.22 + 0.45 * lane_t)
                depth = extents.y * (0.22 + 0.28 * (1.0 - lane_t))

            spawn = (
                center
                + player_forward * depth
                + player_right * lateral
                + Vector((0.0, 0.0, height))
            )
            spawn = _clamp_point_to_box(spawn, center, extents, margin)
            formation_offset = formation_offsets[index] if index < len(formation_offsets) else Vector((0.0, 0.0, 0.0))
            if motion.get("keep_formation") and motion.get("pattern") != "EDGE_ORBIT":
                spawn = _clamp_point_to_box(spawn + formation_offset, center, extents, margin)

            path_id = _unique_enemy_path_id(scene)
            speed_jitter = 1.0 if motion.get("keep_formation") else rng.uniform(0.88, 1.12)
            speed = speed_by_style.get(style, 0.050) * motion.get("speed_multiplier", 1.0) * speed_jitter
            loop = motion["loop"] if motion.get("loop") is not None else style == 'PATROL'
            points = _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, pair_index, rng, formation_offset)
            if motion.get("pattern") == "EDGE_ORBIT" and points:
                spawn = points[0]
            handle_type = 'VECTOR' if motion.get("pattern") == "EDGE_ORBIT" and motion.get("respect_bounds") else 'AUTO'
            path_obj = _create_ai_enemy_path(collection, path_id, points, loop, speed, handle_type)
            path_obj["myaddon_ai_motion_prompt"] = motion_prompt

            trigger_name = ""
            delay_frames = wave_delay
            if index >= opener_count and generated_enemies:
                trigger_enemy = generated_enemies[index - opener_count]
                trigger_name = trigger_enemy.name
                delay_jitter = wave_delay // 5
                delay_frames = max(0, wave_delay + rng.randint(-delay_jitter, delay_jitter))

            enemy_name = f"AIEnemy_{index + 1:02d}"
            enemy_type = "VF1-1" if index % 3 == 2 else "VF1"
            enemy_obj = _create_ai_enemy_spawn(
                collection,
                enemy_name,
                spawn,
                player - spawn,
                enemy_type,
                path_id,
                trigger_name,
                delay_frames,
            )
            enemy_obj["myaddon_ai_motion_prompt"] = motion_prompt

            generated_objects.extend([path_obj, enemy_obj])
            generated_enemies.append(enemy_obj)"""

new_builtin = """        lane_count = max(1, (count + 1) // 2)
        formation_offsets = _build_ai_formation_offsets(count, extents, motion)
        speed_by_style = {
            'BALANCED': 0.050,
            'AMBUSH': 0.060,
            'SWARM': 0.072,
            'PATROL': 0.038,
        }
        opener_count = min(count, 4 if style == 'SWARM' else 2)
        if motion.get("opener") == "ALL":
            opener_count = count
        elif motion.get("opener") == "ONE":
            opener_count = 1
        margin = Vector((2.0, 2.0, 1.2))

        builtin_plan = {"enemies": []}

        for index in range(count):
            pair_index = index // 2
            lane_t = 0.0 if lane_count <= 1 else pair_index / (lane_count - 1)
            side = -1.0 if index % 2 == 0 else 1.0
            z_span = min(extents.z * 0.32, 4.0)
            height = rng.uniform(-z_span, z_span)

            if style == 'AMBUSH':
                lateral = side * extents.x * rng.uniform(0.55, 0.82)
                depth = extents.y * rng.uniform(-0.05, 0.42)
            elif style == 'SWARM':
                swarm_step = min(2.2, extents.x * 0.12)
                lateral = (index - (count - 1) * 0.5) * swarm_step + rng.uniform(-0.7, 0.7)
                depth = extents.y * rng.uniform(0.18, 0.34)
            elif style == 'PATROL':
                lateral = side * extents.x * (0.28 + 0.38 * lane_t)
                depth = extents.y * (0.02 + 0.34 * lane_t)
            else:
                lateral = side * extents.x * (0.22 + 0.45 * lane_t)
                depth = extents.y * (0.22 + 0.28 * (1.0 - lane_t))

            spawn = (
                center
                + player_forward * depth
                + player_right * lateral
                + Vector((0.0, 0.0, height))
            )
            spawn = _clamp_point_to_box(spawn, center, extents, margin)
            formation_offset = formation_offsets[index] if index < len(formation_offsets) else Vector((0.0, 0.0, 0.0))
            if motion.get("keep_formation") and motion.get("pattern") != "EDGE_ORBIT":
                spawn = _clamp_point_to_box(spawn + formation_offset, center, extents, margin)

            speed_jitter = 1.0 if motion.get("keep_formation") else rng.uniform(0.88, 1.12)
            speed = speed_by_style.get(style, 0.050) * motion.get("speed_multiplier", 1.0) * speed_jitter
            loop = motion["loop"] if motion.get("loop") is not None else style == 'PATROL'
            points = _build_ai_enemy_points(style, motion, spawn, player, center, extents, side, pair_index, rng, formation_offset)
            if motion.get("pattern") == "EDGE_ORBIT" and points:
                spawn = points[0]
            handle_type = 'VECTOR' if motion.get("pattern") == "EDGE_ORBIT" and motion.get("respect_bounds") else 'AUTO'

            trigger_index = -1
            delay_frames = wave_delay
            if index >= opener_count:
                trigger_index = index - opener_count
                delay_jitter = wave_delay // 5
                delay_frames = max(0, wave_delay + rng.randint(-delay_jitter, delay_jitter))

            enemy_type = "VF1-1" if index % 3 == 2 else "VF1"

            builtin_plan["enemies"].append({
                "spawn": {"x": spawn.x, "y": spawn.y, "z": spawn.z},
                "path": [{"x": p.x, "y": p.y, "z": p.z} for p in points],
                "enemy_type": enemy_type,
                "loop": loop,
                "speed": speed,
                "trigger_index": trigger_index,
                "delay_frames": delay_frames,
                "handle_type": handle_type,
            })

        blueprints = _sanitize_enemy_plan_data(builtin_plan, count, center, extents, player)
        if getattr(scene, "myaddon_ai_enemy_clear_existing", True):
            _delete_ai_generated_objects(scene)

        generated_objects, generated_enemies = _create_ai_enemy_objects_from_blueprints(
            scene,
            collection,
            blueprints,
            motion_prompt,
            player,
            "BUILTIN",
        )"""
text = text.replace(old_builtin, new_builtin)

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)

print("Update complete!")
