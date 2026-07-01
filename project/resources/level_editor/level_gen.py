import bpy
from mathutils import Vector
import random
import math

def _delete_ai_generated_obstacles(scene):
    objects_to_delete = []
    for obj in scene.objects:
        if obj.name.startswith("AIObstacle_"):
            objects_to_delete.append(obj)
    
    if objects_to_delete:
        bpy.ops.object.select_all(action='DESELECT')
        for obj in objects_to_delete:
            obj.select_set(True)
        bpy.ops.object.delete()

def _parse_ai_level_prompt(prompt):
    normalized = str(prompt or "").casefold()
    level_plan = {
        "style": "RANDOM",
        "shape": "CUBE",
        "density_multiplier": 1.0,
    }

    # Style
    if _prompt_has(normalized, "市街地", "ビル", "街", "city", "grid"):
        level_plan["style"] = "CITY"
        level_plan["shape"] = "PILLAR"
    elif _prompt_has(normalized, "迷路", "メイズ", "maze"):
        level_plan["style"] = "MAZE"
        level_plan["shape"] = "WALL"
    elif _prompt_has(normalized, "防衛線", "ライン", "横一列", "line", "defense"):
        level_plan["style"] = "DEFENSE_LINE"
        level_plan["shape"] = "WALL"
    elif _prompt_has(normalized, "アリーナ", "中央", "囲む", "arena"):
        level_plan["style"] = "ARENA"

    # Shape
    if _prompt_has(normalized, "壁", "長い", "wall"):
        level_plan["shape"] = "WALL"
    elif _prompt_has(normalized, "柱", "高い", "タワー", "pillar", "tower"):
        level_plan["shape"] = "PILLAR"
    elif _prompt_has(normalized, "小さい", "散らば", "散らす", "ブロック", "block"):
        level_plan["shape"] = "BLOCK"

    # Density
    import re
    custom_mult = None
    mult_match = re.search(r'(?:[xXｘＸ×*]\s*(\d+(?:\.\d+)?))|(?:(\d+(?:\.\d+)?)\s*(?:倍|[xXｘＸ×*]))', prompt)
    if mult_match:
        try:
            custom_mult = float(mult_match.group(1) or mult_match.group(2))
        except Exception:
            pass

    if custom_mult is not None:
        level_plan["density_multiplier"] = custom_mult
    elif _prompt_has(normalized, "多く", "たくさん", "密集", "many"):
        level_plan["density_multiplier"] = 2.0
    elif _prompt_has(normalized, "少なく", "スカスカ", "few"):
        level_plan["density_multiplier"] = 0.5

    return level_plan

class MYADDON_OT_ai_generate_level_obstacles(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_generate_level_obstacles"
    bl_label = "AIで障害物を自動配置"
    bl_description = "コンセプトに合わせてフィールド内に障害物を自動生成・配置します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        scene = context.scene
        
        prompt = getattr(scene, "myaddon_ai_level_prompt", "")
        seed = int(getattr(scene, "myaddon_ai_level_seed", 1))
        clear_existing = getattr(scene, "myaddon_ai_level_clear_existing", True)
        
        if clear_existing:
            _delete_ai_generated_obstacles(scene)
            
        plan = _parse_ai_level_prompt(prompt)
        rng = random.Random(seed)
        
        # フィールドの取得
        center = Vector((0,0,0))
        extents = Vector((20,20,20))
        # operators.py 内の _stage_bounds_box はすでに定義済みなので呼び出せる前提
        try:
            from . import operators
            center, extents = operators._stage_bounds_box(scene)
        except:
            # Fallback inline if import fails inside operators.py itself
            for obj in scene.objects:
                if obj.name == "StageBounds":
                    center = obj.location
                    extents = Vector((obj.scale.x * 2.0, obj.scale.y * 2.0, obj.scale.z * 2.0))
                    break
        
        style = plan["style"]
        shape = plan["shape"]
        density = plan["density_multiplier"]
        
        obstacles_to_create = [] # list of dicts: location, scale
        
        base_count = 10
        if style == "CITY":
            base_count = int(16 * density)
            cols = max(2, int(math.sqrt(base_count)))
            rows = max(2, int(base_count / cols))
            spacing_x = (extents.x * 2 * 0.8) / cols
            spacing_y = (extents.y * 2 * 0.8) / rows
            
            for r in range(rows):
                for c in range(cols):
                    if rng.random() > 0.3: # 70% chance to place a building
                        lx = center.x - extents.x*0.8 + c * spacing_x + rng.uniform(-2, 2)
                        ly = center.y - extents.y*0.8 + r * spacing_y + rng.uniform(-2, 2)
                        obstacles_to_create.append({
                            "loc": Vector((lx, ly, 0)),
                            "scale": Vector((rng.uniform(1.0, 3.0), rng.uniform(1.0, 3.0), rng.uniform(4.0, 12.0)))
                        })
        elif style == "DEFENSE_LINE":
            base_count = int(8 * density)
            spacing = (extents.x * 2 * 0.9) / max(1, base_count)
            for i in range(base_count):
                lx = center.x - extents.x*0.9 + i * spacing
                ly = center.y + extents.y * rng.uniform(0.1, 0.3)
                obstacles_to_create.append({
                    "loc": Vector((lx, ly + rng.uniform(-1, 1), 0)),
                    "scale": Vector((rng.uniform(2.0, 6.0), rng.uniform(1.0, 2.0), rng.uniform(2.0, 4.0)))
                })
        elif style == "MAZE":
            base_count = int(12 * density)
            for i in range(base_count):
                lx = center.x + rng.uniform(-extents.x*0.8, extents.x*0.8)
                ly = center.y + rng.uniform(-extents.y*0.8, extents.y*0.8)
                is_horizontal = rng.random() > 0.5
                sx = rng.uniform(6.0, 15.0) if is_horizontal else rng.uniform(1.0, 2.0)
                sy = rng.uniform(1.0, 2.0) if is_horizontal else rng.uniform(6.0, 15.0)
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((sx, sy, rng.uniform(3.0, 6.0)))
                })
        elif style == "ARENA":
            base_count = int(12 * density)
            for i in range(base_count):
                angle = (i / base_count) * 2 * math.pi
                radius_x = extents.x * rng.uniform(0.7, 0.95)
                radius_y = extents.y * rng.uniform(0.7, 0.95)
                lx = center.x + math.cos(angle) * radius_x
                ly = center.y + math.sin(angle) * radius_y
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((rng.uniform(2.0, 5.0), rng.uniform(2.0, 5.0), rng.uniform(2.0, 8.0)))
                })
        else: # RANDOM
            base_count = int(15 * density)
            for i in range(base_count):
                lx = center.x + rng.uniform(-extents.x*0.9, extents.x*0.9)
                ly = center.y + rng.uniform(-extents.y*0.9, extents.y*0.9)
                
                if shape == "WALL":
                    sx, sy, sz = rng.uniform(4.0, 10.0), rng.uniform(1.0, 2.0), rng.uniform(2.0, 5.0)
                elif shape == "PILLAR":
                    sx, sy, sz = rng.uniform(1.0, 3.0), rng.uniform(1.0, 3.0), rng.uniform(5.0, 15.0)
                elif shape == "BLOCK":
                    sx, sy, sz = rng.uniform(1.0, 3.0), rng.uniform(1.0, 3.0), rng.uniform(1.0, 3.0)
                else: # CUBE / Default
                    sx, sy, sz = rng.uniform(2.0, 5.0), rng.uniform(2.0, 5.0), rng.uniform(2.0, 5.0)
                    
                obstacles_to_create.append({
                    "loc": Vector((lx, ly, 0)),
                    "scale": Vector((sx, sy, sz))
                })

        # Get or create collection
        collection = scene.collection
        for coll in bpy.data.collections:
            if coll.name == "AI_Obstacles":
                collection = coll
                break
        else:
            collection = bpy.data.collections.new("AI_Obstacles")
            scene.collection.children.link(collection)

        created_objs = []
        for i, obs in enumerate(obstacles_to_create):
            bpy.ops.mesh.primitive_cube_add(size=2.0)
            obj = context.active_object
            obj.name = f"AIObstacle_{i+1:03d}"
            
            # Remove from all collections and link to AI_Obstacles
            for coll in obj.users_collection:
                coll.objects.unlink(obj)
            collection.objects.link(obj)
            
            # Position and Scale
            obj.location = obs["loc"]
            # Z location adjust to sit on the floor (assuming center.z - extents.z is floor)
            floor_z = center.z - extents.z
            obj.location.z = floor_z + obs["scale"].z # size=2.0, so scale.z is the height
            
            obj.scale = obs["scale"]
            
            # Game Property
            obj["game_obj_type"] = "OBSTACLE"
            obj.game_obj_type = "OBSTACLE"
            
            created_objs.append(obj)
            
        self.report({'INFO'}, f"{len(created_objs)}個のAI障害物を配置しました")
        return {'FINISHED'}
