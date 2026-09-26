"""Create a player-inspired boss fighter for CG2.

Run with Blender 4.x:
  blender --background --python project/tools/create_boss_fighter.py
"""

from pathlib import Path

import bpy
from mathutils import Vector


PROJECT_DIR = Path(__file__).resolve().parents[1]
SOURCE_OBJ = PROJECT_DIR / "resources" / "player_fighter.obj"
OUTPUT_OBJ = PROJECT_DIR / "resources" / "BossFighter.obj"
OUTPUT_BLEND = PROJECT_DIR / "resources" / "models" / "BossFighter.blend"
OUTPUT_PREVIEW = PROJECT_DIR / "resources" / "models" / "BossFighter_preview.png"


def make_material(name, color, metallic=0.0, roughness=0.5):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return material


def assign_material(obj, material):
    obj.data.materials.clear()
    obj.data.materials.append(material)


def activate(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def add_bevel(obj, width=0.08, segments=2):
    modifier = obj.modifiers.new("Edge bevel", "BEVEL")
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = "ANGLE"
    activate(obj)
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def cube(name, location, dimensions, material, rotation=(0.0, 0.0, 0.0), bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(location=location, rotation=rotation)
    obj = bpy.context.active_object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        add_bevel(obj, bevel)
    assign_material(obj, material)
    return obj


def cylinder(name, location, radius, depth, material, rotation=(0.0, 0.0, 0.0)):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=16, radius=radius, depth=depth, location=location, rotation=rotation
    )
    obj = bpy.context.active_object
    obj.name = name
    assign_material(obj, material)
    bevel = obj.modifiers.new("Edge bevel", "BEVEL")
    bevel.width = min(radius * 0.22, 0.06)
    bevel.segments = 2
    activate(obj)
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    return obj


def triangular_fin(name, x_sign, material):
    # A vertical triangular stabilizer, symmetric across the X axis.
    x_root = 0.55 * x_sign
    x_tip = 1.25 * x_sign
    vertices = [
        (x_root, -0.08, -0.75),
        (x_root, -0.08, 0.68),
        (x_tip, -0.08, -0.18),
        (x_root, 0.08, -0.75),
        (x_root, 0.08, 0.68),
        (x_tip, 0.08, -0.18),
    ]
    faces = [(0, 1, 2), (3, 5, 4), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)]
    mesh = bpy.data.meshes.new(f"{name}Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(material)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def point_at(obj, target):
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_preview_setup():
    bpy.ops.object.camera_add(location=(13.5, -18.0, 9.5))
    camera = bpy.context.active_object
    camera.name = "BossFighter_PreviewCamera"
    camera.data.lens = 53
    point_at(camera, (0.0, 0.0, -0.15))
    bpy.context.scene.camera = camera

    for name, location, energy, color, size in (
        ("BossFighter_KeyLight", (3.5, -4.0, 6.5), 1600, (0.72, 0.86, 1.0), 5.0),
        ("BossFighter_RimLight", (-5.5, 2.0, 3.5), 1200, (1.0, 0.12, 0.06), 4.0),
    ):
        bpy.ops.object.light_add(type="AREA", location=location)
        light = bpy.context.active_object
        light.name = name
        light.data.energy = energy
        light.data.color = color
        light.data.shape = "DISK"
        light.data.size = size
        point_at(light, (0.0, 0.0, 0.0))


def create_boss():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    OUTPUT_OBJ.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_BLEND.parent.mkdir(parents=True, exist_ok=True)

    armor = make_material("Boss Armor", (0.075, 0.10, 0.15), metallic=0.82, roughness=0.28)
    panel = make_material("Boss Red Panels", (0.48, 0.018, 0.025), metallic=0.55, roughness=0.32)
    glow = make_material("Boss Cyan Core", (0.02, 0.55, 0.95), metallic=0.18, roughness=0.16)
    gold = make_material("Boss Gold Trim", (0.75, 0.27, 0.035), metallic=0.80, roughness=0.25)

    bpy.ops.wm.obj_import(filepath=str(SOURCE_OBJ))
    player_base = bpy.context.active_object
    player_base.name = "BossFighter_PlayerDerivedCore"
    player_base.scale = (2.45, 2.45, 2.45)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign_material(player_base, armor)

    # Wide armored wings and a heavy centerline shell keep the recognizable fighter silhouette.
    cube("BossFighter_CenterArmor", (0.0, 0.0, 0.05), (1.45, 0.64, 2.55), armor, bevel=0.12)
    cube("BossFighter_LeftWing", (-3.0, 0.0, -0.15), (4.9, 0.26, 1.25), armor, rotation=(0.0, 0.22, -0.08), bevel=0.10)
    cube("BossFighter_RightWing", (3.0, 0.0, -0.15), (4.9, 0.26, 1.25), armor, rotation=(0.0, -0.22, 0.08), bevel=0.10)
    cube("BossFighter_LeftWingPanel", (-3.2, -0.16, 0.16), (3.7, 0.09, 0.45), panel, rotation=(0.0, 0.22, -0.08), bevel=0.04)
    cube("BossFighter_RightWingPanel", (3.2, -0.16, 0.16), (3.7, 0.09, 0.45), panel, rotation=(0.0, -0.22, 0.08), bevel=0.04)

    # Command bridge, reactor, and crest visually mark it as a boss rather than a scaled player ship.
    cube("BossFighter_CommandBridge", (0.0, -0.34, 0.58), (0.85, 0.32, 1.05), gold, rotation=(0.0, 0.18, 0.0), bevel=0.08)
    cube("BossFighter_ReactorHousing", (0.0, 0.0, -1.25), (1.22, 0.68, 0.65), panel, bevel=0.10)
    cylinder("BossFighter_ReactorCore", (0.0, -0.38, -1.33), 0.30, 0.14, glow, rotation=(1.5708, 0.0, 0.0))
    triangular_fin("BossFighter_LeftCrest", -1.0, gold)
    triangular_fin("BossFighter_RightCrest", 1.0, gold)

    # Four railguns and rear engine pods create the boss' heavier weapon silhouette.
    for side in (-1.0, 1.0):
        for index, z in enumerate((0.58, -0.58)):
            x = side * 2.05
            cylinder(
                f"BossFighter_Railgun_{'L' if side < 0 else 'R'}_{index + 1}",
                (x, -0.02, z),
                0.20,
                2.2,
                gold,
                rotation=(0.0, 1.5708, 0.0),
            )
            cylinder(
                f"BossFighter_RailgunGlow_{'L' if side < 0 else 'R'}_{index + 1}",
                (side * 3.14, -0.02, z),
                0.11,
                0.16,
                glow,
                rotation=(0.0, 1.5708, 0.0),
            )

        cylinder(
            f"BossFighter_EnginePod_{'L' if side < 0 else 'R'}",
            (side * 1.0, 0.0, -1.65),
            0.43,
            1.15,
            armor,
        )
        cylinder(
            f"BossFighter_EngineGlow_{'L' if side < 0 else 'R'}",
            (side * 1.0, 0.0, -2.25),
            0.27,
            0.08,
            glow,
        )

    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            for polygon in obj.data.polygons:
                polygon.use_smooth = False

    bpy.context.scene.render.engine = "BLENDER_EEVEE_NEXT"
    if bpy.context.scene.world is None:
        bpy.context.scene.world = bpy.data.worlds.new("Boss Fighter World")
    bpy.context.scene.world.color = (0.015, 0.02, 0.04)
    add_preview_setup()
    bpy.context.scene.render.resolution_x = 960
    bpy.context.scene.render.resolution_y = 720
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.render.image_settings.file_format = "PNG"
    bpy.context.scene.render.filepath = str(OUTPUT_PREVIEW)
    bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_BLEND), check_existing=False)

    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            obj.select_set(True)
    bpy.context.view_layer.objects.active = player_base
    bpy.ops.wm.obj_export(
        filepath=str(OUTPUT_OBJ),
        export_selected_objects=True,
        export_materials=True,
        export_uv=True,
        export_normals=True,
        export_triangulated_mesh=True,
        apply_modifiers=True,
        forward_axis="NEGATIVE_Z",
        up_axis="Y",
    )


if __name__ == "__main__":
    create_boss()
