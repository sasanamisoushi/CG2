"""Create an original giant transformable-space-battleship boss for CG2."""

from pathlib import Path
from math import radians

import bpy
from mathutils import Vector


PROJECT_DIR = Path(__file__).resolve().parents[1]
OUTPUT_OBJ = PROJECT_DIR / "resources" / "FortressBattleshipBoss.obj"
OUTPUT_BLEND = PROJECT_DIR / "resources" / "models" / "FortressBattleshipBoss.blend"
OUTPUT_PREVIEW = PROJECT_DIR / "resources" / "models" / "FortressBattleshipBoss_preview.png"


def material(name, color, metallic=0.0, roughness=0.45, emission=None):
    result = bpy.data.materials.new(name)
    result.diffuse_color = (*color, 1.0)
    result.use_nodes = True
    bsdf = result.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emission:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 4.0
    return result


def select_only(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def finish_mesh(obj, mat, bevel=0.0):
    obj.data.materials.append(mat)
    if bevel:
        modifier = obj.modifiers.new("Armor edge bevel", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
        modifier.limit_method = "ANGLE"
        select_only(obj)
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


def box(name, location, dimensions, mat, bevel=0.0, rotation=(0.0, 0.0, 0.0)):
    bpy.ops.mesh.primitive_cube_add(location=location, rotation=rotation)
    obj = bpy.context.active_object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish_mesh(obj, mat, bevel)


def cylinder(name, location, radius, depth, mat, rotation=(0.0, 0.0, 0.0)):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=16, radius=radius, depth=depth, location=location, rotation=rotation
    )
    obj = bpy.context.active_object
    obj.name = name
    return finish_mesh(obj, mat, min(radius * 0.18, 0.18))


def wedge(name, location, width_back, width_front, height_back, height_front, length, mat):
    """A tapered armored hull section along local Z."""
    back_z, front_z = -length * 0.5, length * 0.5
    vertices = [
        (-width_back * 0.5, -height_back * 0.5, back_z),
        ( width_back * 0.5, -height_back * 0.5, back_z),
        ( width_back * 0.5,  height_back * 0.5, back_z),
        (-width_back * 0.5,  height_back * 0.5, back_z),
        (-width_front * 0.5, -height_front * 0.5, front_z),
        ( width_front * 0.5, -height_front * 0.5, front_z),
        ( width_front * 0.5,  height_front * 0.5, front_z),
        (-width_front * 0.5,  height_front * 0.5, front_z),
    ]
    faces = [
        (0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1),
        (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0),
    ]
    mesh = bpy.data.meshes.new(f"{name}Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.location = location
    return finish_mesh(obj, mat, 0.16)


def point_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def add_turret(name, x, z, armor, trim, glow):
    cylinder(f"{name}_Base", (x, 2.15, z), 1.25, 0.60, armor, rotation=(1.5708, 0.0, 0.0))
    box(f"{name}_Housing", (x, 2.85, z + 0.25), (2.2, 1.1, 2.4), trim, bevel=0.12)
    for side in (-0.48, 0.48):
        cylinder(
            f"{name}_Cannon_{'L' if side < 0 else 'R'}",
            (x + side, 3.05, z + 2.8), 0.28, 4.2, armor,
        )
        cylinder(
            f"{name}_Muzzle_{'L' if side < 0 else 'R'}",
            (x + side, 3.05, z + 4.93), 0.34, 0.20, glow,
        )


def create_model():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    OUTPUT_OBJ.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_BLEND.parent.mkdir(parents=True, exist_ok=True)

    armor = material("Fortress pearl armor", (0.38, 0.47, 0.58), metallic=0.70, roughness=0.27)
    dark = material("Fortress armored seams", (0.035, 0.055, 0.09), metallic=0.88, roughness=0.22)
    red = material("Fortress command red", (0.52, 0.02, 0.035), metallic=0.55, roughness=0.28)
    gold = material("Fortress gold trim", (0.82, 0.34, 0.04), metallic=0.82, roughness=0.22)
    glow = material("Fortress ion glow", (0.01, 0.42, 1.0), metallic=0.10, roughness=0.12, emission=(0.01, 0.42, 1.0))

    # Long, split-hull assault carrier: a narrow spear-like bow, central bridge and two rear engine blocks.
    # The proportions deliberately emphasize the long silhouette in the supplied reference.
    wedge("Fortress_MainHull", (0.0, 0.0, 2.0), 15.0, 3.0, 5.8, 1.4, 62.0, armor)
    wedge("Fortress_BowBlade", (0.0, -0.35, 43.0), 6.5, 0.45, 1.7, 0.25, 28.0, dark)
    wedge("Fortress_BowCrest", (0.0, 1.7, 31.0), 6.8, 1.1, 1.2, 0.25, 28.0, armor)
    box("Fortress_BowSpine", (0.0, 2.15, 35.0), (1.45, 0.65, 31.0), dark, bevel=0.10)
    cylinder("Fortress_BowMuzzle", (0.0, 2.15, 50.65), 0.55, 0.35, glow)

    # Split forward armor makes the hull look like an open mechanical beak rather than a simple airplane.
    for side in (-1.0, 1.0):
        label = "L" if side < 0 else "R"
        wedge(f"Fortress_{label}_ForwardSponson", (side * 8.2, 0.25, 15.0), 7.2, 1.1, 2.5, 0.55, 47.0, armor)
        wedge(f"Fortress_{label}_ForwardEdge", (side * 10.4, -0.25, 21.0), 2.0, 0.4, 0.95, 0.2, 42.0, dark)
        box(f"Fortress_{label}_ForwardGun", (side * 5.9, 1.7, 27.0), (0.75, 0.75, 29.0), dark, bevel=0.08)
        cylinder(f"Fortress_{label}_ForwardMuzzle", (side * 5.9, 1.7, 41.65), 0.36, 0.20, glow)

    # Command island and layered deck just behind the bow.
    box("Fortress_CentralDeck", (0.0, 3.0, -2.5), (15.0, 0.70, 27.0), dark, bevel=0.18)
    wedge("Fortress_CentralArmor", (0.0, 4.05, -1.0), 10.5, 4.5, 2.4, 1.1, 21.0, armor)
    box("Fortress_Bridge", (0.0, 6.4, -8.5), (5.8, 3.1, 7.0), red, bevel=0.26)
    wedge("Fortress_BridgeCrown", (0.0, 8.55, -8.5), 5.2, 1.2, 1.55, 0.35, 7.6, dark)
    box("Fortress_BridgeWindow", (0.0, 6.55, -4.95), (3.6, 1.05, 0.22), glow, bevel=0.05)
    box("Fortress_BridgeAntenna", (0.0, 10.0, -10.5), (0.45, 3.5, 0.45), gold, bevel=0.05)

    # Broad rear outriggers and paired thruster pods, separated from the central hull.
    for side in (-1.0, 1.0):
        label = "L" if side < 0 else "R"
        wedge(f"Fortress_{label}_RearWing", (side * 17.5, 0.0, -9.0), 10.0, 3.2, 3.0, 1.1, 34.0, armor)
        box(f"Fortress_{label}_RearDeck", (side * 18.0, 2.0, -10.0), (7.5, 0.65, 21.0), dark, bevel=0.16)
        box(f"Fortress_{label}_Nacelle", (side * 21.0, -0.2, -25.5), (8.0, 5.0, 25.0), armor, bevel=0.25)
        wedge(f"Fortress_{label}_NacelleArmor", (side * 21.0, 2.7, -21.0), 7.2, 3.1, 1.0, 0.35, 15.0, dark)
        box(f"Fortress_{label}_NacelleStripe", (side * 21.0, -2.85, -24.0), (6.5, 0.16, 15.0), red, bevel=0.04)
        box(f"Fortress_{label}_WingTip", (side * 29.0, -0.25, -8.0), (12.0, 0.80, 10.0), armor, bevel=0.15)
        box(f"Fortress_{label}_WingTrim", (side * 29.0, -0.70, -6.5), (9.6, 0.14, 1.1), gold, bevel=0.03)
        for engine in (-1.8, 1.8):
            cylinder(f"Fortress_{label}_Engine_{engine}", (side * 21.0 + engine, -0.2, -39.2), 1.55, 5.3, dark)
            cylinder(f"Fortress_{label}_EngineGlow_{engine}", (side * 21.0 + engine, -0.2, -41.9), 1.13, 0.28, glow)

    # Artillery and a glowing ventral hangar complete the boss-readable combat detail.
    add_turret("Fortress_TurretLeft", -7.0, 1.0, armor, red, glow)
    add_turret("Fortress_TurretCenter", 0.0, 4.0, armor, red, glow)
    add_turret("Fortress_TurretRight", 7.0, 1.0, armor, red, glow)
    box("Fortress_VentralHangar", (0.0, -3.1, -5.5), (8.6, 0.45, 18.0), dark, bevel=0.10)
    for x in (-3.2, -1.05, 1.05, 3.2):
        box(f"Fortress_HangarLight_{x}", (x, -3.38, -5.5), (0.46, 0.10, 12.0), glow, bevel=0.02)

    if bpy.context.scene.world is None:
        bpy.context.scene.world = bpy.data.worlds.new("Fortress preview world")
    bpy.context.scene.world.use_nodes = True
    world_background = bpy.context.scene.world.node_tree.nodes.get("Background")
    world_background.inputs["Color"].default_value = (0.008, 0.018, 0.045, 1.0)
    world_background.inputs["Strength"].default_value = 0.42
    bpy.context.scene.render.engine = "BLENDER_EEVEE_NEXT"
    bpy.context.scene.render.resolution_x = 960
    bpy.context.scene.render.resolution_y = 720
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.render.image_settings.file_format = "PNG"
    bpy.context.scene.render.filepath = str(OUTPUT_PREVIEW)
    bpy.context.scene.view_settings.look = "AgX - Medium High Contrast"
    bpy.context.scene.view_settings.exposure = 1.8

    bpy.ops.object.camera_add(location=(122.0, 108.0, 8.0))
    camera = bpy.context.active_object
    camera.name = "Fortress_PreviewCamera"
    camera.data.lens = 46
    point_at(camera, (0.0, 0.0, 2.0))
    # The gameplay model is authored along Z; roll only the preview camera so the ship reads left-to-right.
    camera.rotation_euler.rotate_axis("Z", radians(90.0))
    bpy.context.scene.camera = camera
    for name, location, energy, color, size in (
        ("Fortress_KeyLight", (62.0, 38.0, 52.0), 26000, (0.70, 0.84, 1.0), 34.0),
        ("Fortress_FillLight", (-55.0, 26.0, 15.0), 18000, (0.26, 0.48, 1.0), 28.0),
        ("Fortress_RimLight", (-15.0, -34.0, 56.0), 15000, (1.0, 0.10, 0.035), 26.0),
    ):
        bpy.ops.object.light_add(type="AREA", location=location)
        light = bpy.context.active_object
        light.name = name
        light.data.energy = energy
        light.data.color = color
        light.data.shape = "DISK"
        light.data.size = size
        point_at(light, (0.0, 0.0, 0.0))

    bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_BLEND), check_existing=False)

    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            obj.select_set(True)
    bpy.context.view_layer.objects.active = bpy.data.objects["Fortress_MainHull"]
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
    create_model()
