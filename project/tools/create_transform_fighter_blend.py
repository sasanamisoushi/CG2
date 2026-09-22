"""Create the runtime TransformFighter model as a Blender scene and GLB asset.

Run with Blender in background mode.  The three poses match the game-side
procedural model: frame 1 is Fighter, frame 41 is Gerwalk, and frame 81 is
Battroid.  The scene intentionally uses an original, logo-free design.
"""

from pathlib import Path
import math

import bpy
from mathutils import Euler, Matrix, Vector


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = ROOT / "resources" / "models"
BLEND_PATH = OUTPUT_DIR / "TransformFighter.blend"
GLB_PATH = OUTPUT_DIR / "TransformFighter.glb"
PREVIEW_PATH = OUTPUT_DIR / "TransformFighter_preview.png"
MODEL_SCALE = 0.08
FRAMES = (1, 41, 81)
STYLE_TRANSLATE = (0.74, 1.32, 0.88)
STYLE_SCALE = (0.70, 1.12, 0.78)


def pose(x, y, z, sx, sy, sz, rx=0.0, ry=0.0, rz=0.0):
    return ((x, y, z), (sx, sy, sz), (rx, ry, rz))


MATERIALS = {
    "Armor": (0.82, 0.86, 0.91, 1.0),
    "Frame": (0.06, 0.08, 0.12, 1.0),
    "Accent": (0.86, 0.04, 0.06, 1.0),
    "Visor": (0.03, 0.90, 0.72, 1.0),
    "Engine": (0.10, 0.12, 0.16, 1.0),
}


PARTS = [
    ("Core", "Frame", "box", [
        pose(0, 0, 0, 5.0, 1.6, 8.0),
        pose(0, 3.8, 0, 3.8, 1.6, 4.8),
        pose(0, 5.2, 0, 2.6, 2.0, 1.6),
    ]),
    ("Chest", "Armor", "box", [
        pose(0, 0.8, 1.2, 4.6, 1.4, 3.2), pose(0, 5.0, 0.6, 4.2, 1.8, 3.0), pose(0, 7.2, 0, 4.2, 2.1, 2.2),
    ]),
    ("ChestAccent", "Accent", "box", [
        pose(0, 1.0, 3.4, 0.65, 0.65, 2.0), pose(0, 5.1, 2.5, 0.65, 0.8, 1.3), pose(0, 7.4, 1.9, 0.65, 1.0, 0.25),
    ]),
    ("Head", "Armor", "box", [
        pose(0, 0.8, 4.0, 1.35, 1.05, 1.5), pose(0, 6.9, 1.6, 1.35, 1.15, 1.4), pose(0, 10.0, 0, 1.35, 1.35, 1.35),
    ]),
    ("Visor", "Visor", "box", [
        pose(0, 0.8, 5.3, 0.9, 0.35, 0.24), pose(0, 6.9, 2.95, 0.9, 0.35, 0.24), pose(0, 10.0, 1.25, 0.9, 0.35, 0.24),
    ]),
    ("Crest", "Accent", "box", [
        pose(0, 1.8, 4.0, 0.3, 1.5, 0.6), pose(0, 8.2, 1.6, 0.3, 1.5, 0.6), pose(0, 11.7, 0, 0.3, 1.5, 0.6),
    ]),
    ("LeftShoulder", "Armor", "box", [
        pose(-5.0, 0, 0.4, 2.3, 0.75, 4.8, 0, 0, -0.18), pose(-4.6, 5.4, 0.2, 2.1, 1.4, 2.8, 0, 0, -0.4), pose(-5.0, 7.5, 0, 2.0, 1.7, 1.8),
    ]),
    ("RightShoulder", "Armor", "box", [
        pose(5.0, 0, 0.4, 2.3, 0.75, 4.8, 0, 0, 0.18), pose(4.6, 5.4, 0.2, 2.1, 1.4, 2.8, 0, 0, 0.4), pose(5.0, 7.5, 0, 2.0, 1.7, 1.8),
    ]),
    ("LeftForearm", "Frame", "box", [
        pose(-7.2, -0.3, 0.4, 1.1, 0.75, 5.5, 0, 0, -0.18), pose(-5.9, 3.2, 1.2, 1.25, 1.1, 3.0, -0.45, 0, -0.22), pose(-6.0, 4.4, 0.8, 1.25, 2.8, 1.25, 0, 0, -0.08),
    ]),
    ("RightForearm", "Frame", "box", [
        pose(7.2, -0.3, 0.4, 1.1, 0.75, 5.5, 0, 0, 0.18), pose(5.9, 3.2, 1.2, 1.25, 1.1, 3.0, -0.45, 0, 0.22), pose(6.0, 4.4, 0.8, 1.25, 2.8, 1.25, 0, 0, 0.08),
    ]),
    ("LeftHand", "Armor", "box", [
        pose(-8.0, -0.4, 2.6, 1.2, 0.7, 1.8), pose(-6.4, 2.2, 2.5, 1.2, 0.8, 1.3), pose(-6.0, 1.2, 1.0, 1.15, 1.0, 1.0),
    ]),
    ("RightHand", "Armor", "box", [
        pose(8.0, -0.4, 2.6, 1.2, 0.7, 1.8), pose(6.4, 2.2, 2.5, 1.2, 0.8, 1.3), pose(6.0, 1.2, 1.0, 1.15, 1.0, 1.0),
    ]),
    ("LeftHip", "Armor", "box", [
        pose(-2.5, -0.5, -3.0, 2.0, 1.1, 4.0, -0.35), pose(-2.2, 1.8, -1.8, 1.9, 1.8, 2.7, -0.3), pose(-2.4, 3.8, 0, 1.8, 1.8, 1.8),
    ]),
    ("RightHip", "Armor", "box", [
        pose(2.5, -0.5, -3.0, 2.0, 1.1, 4.0, -0.35), pose(2.2, 1.8, -1.8, 1.9, 1.8, 2.7, -0.3), pose(2.4, 3.8, 0, 1.8, 1.8, 1.8),
    ]),
    ("LeftShin", "Armor", "box", [
        pose(-2.5, -0.6, -6.2, 1.6, 0.9, 3.6, -0.25), pose(-2.2, -1.1, -2.4, 1.45, 3.4, 1.5, -0.12), pose(-2.4, -1.2, 0.3, 1.5, 4.0, 1.65),
    ]),
    ("RightShin", "Armor", "box", [
        pose(2.5, -0.6, -6.2, 1.6, 0.9, 3.6, -0.25), pose(2.2, -1.1, -2.4, 1.45, 3.4, 1.5, -0.12), pose(2.4, -1.2, 0.3, 1.5, 4.0, 1.65),
    ]),
    ("LeftFoot", "Frame", "box", [
        pose(-2.5, -0.8, -9.0, 1.7, 0.65, 2.7), pose(-2.2, -4.4, -0.8, 1.8, 0.7, 2.7), pose(-2.4, -5.5, 1.5, 1.9, 0.75, 2.8),
    ]),
    ("RightFoot", "Frame", "box", [
        pose(2.5, -0.8, -9.0, 1.7, 0.65, 2.7), pose(2.2, -4.4, -0.8, 1.8, 0.7, 2.7), pose(2.4, -5.5, 1.5, 1.9, 0.75, 2.8),
    ]),
    ("LeftWing", "Armor", "box", [
        pose(-7.0, 0, -1.5, 4.8, 0.35, 5.0, 0, 0, -0.15), pose(-5.5, 4.4, -1.8, 3.8, 0.5, 3.8, 0, 0, -0.45), pose(-4.1, 6.0, -2.6, 1.2, 2.0, 3.6, 0, 0, -0.24),
    ]),
    ("RightWing", "Armor", "box", [
        pose(7.0, 0, -1.5, 4.8, 0.35, 5.0, 0, 0, 0.15), pose(5.5, 4.4, -1.8, 3.8, 0.5, 3.8, 0, 0, 0.45), pose(4.1, 6.0, -2.6, 1.2, 2.0, 3.6, 0, 0, 0.24),
    ]),
    ("LeftEngine", "Engine", "cylinder", [
        pose(-2.6, 0.1, -6.5, 1.4, 2.8, 1.4, math.pi / 2), pose(-2.7, 3.4, -4.0, 1.4, 2.8, 1.4, 0.55), pose(-2.8, 6.0, -3.5, 1.4, 2.8, 1.4, 0.18),
    ]),
    ("RightEngine", "Engine", "cylinder", [
        pose(2.6, 0.1, -6.5, 1.4, 2.8, 1.4, math.pi / 2), pose(2.7, 3.4, -4.0, 1.4, 2.8, 1.4, 0.55), pose(2.8, 6.0, -3.5, 1.4, 2.8, 1.4, 0.18),
    ]),
]

# Secondary armor is deliberately split into many pieces: it gives the model
# a layered variable-fighter silhouette instead of a box-robot appearance.
PARTS.extend([
    ("CockpitCanopy", "Visor", "box", [pose(0, .9, 5.8, 1.8, .75, 2.4), pose(0, 6.8, 3.5, 1.6, .8, 1.8), pose(0, 8.0, 2.05, 1.65, 1.25, .35)]),
    ("LeftIntake", "Frame", "box", [pose(-2.5, .4, 2.4, 1.15, .8, 2.6), pose(-2.4, 5.0, 1.5, 1.05, 1.1, 1.6), pose(-2.55, 7.0, 1.45, 1.0, 1.1, .45)]),
    ("RightIntake", "Frame", "box", [pose(2.5, .4, 2.4, 1.15, .8, 2.6), pose(2.4, 5.0, 1.5, 1.05, 1.1, 1.6), pose(2.55, 7.0, 1.45, 1.0, 1.1, .45)]),
    ("LeftShoulderPod", "Armor", "box", [pose(-5.5, .4, -3.6, 2.0, 1.1, 3.2, 0, 0, -.24), pose(-5.0, 6.4, -1.5, 2.0, 1.6, 2.0, 0, 0, -.32), pose(-5.25, 8.7, -1.4, 2.15, 1.7, 1.55, 0, 0, -.16)]),
    ("RightShoulderPod", "Armor", "box", [pose(5.5, .4, -3.6, 2.0, 1.1, 3.2, 0, 0, .24), pose(5.0, 6.4, -1.5, 2.0, 1.6, 2.0, 0, 0, .32), pose(5.25, 8.7, -1.4, 2.15, 1.7, 1.55, 0, 0, .16)]),
    ("LeftBicep", "Accent", "box", [pose(-6.2, -.2, 1.3, .55, .6, 3.0), pose(-5.2, 4.2, .7, .65, 1.7, .7, -.3), pose(-5.75, 5.9, .45, .65, 1.65, .7)]),
    ("RightBicep", "Accent", "box", [pose(6.2, -.2, 1.3, .55, .6, 3.0), pose(5.2, 4.2, .7, .65, 1.7, .7, -.3), pose(5.75, 5.9, .45, .65, 1.65, .7)]),
    ("Waist", "Frame", "box", [pose(0, -.2, -1.8, 2.8, .85, 2.5), pose(0, 3.0, -.8, 2.7, 1.15, 1.9), pose(0, 4.8, 0, 2.35, .85, 1.55)]),
    ("FrontSkirt", "Armor", "box", [pose(0, -.7, -3.9, 2.6, .7, 1.3), pose(0, 1.7, .2, 2.5, 1.65, .65), pose(0, 3.9, 1.5, 2.7, 1.55, .5)]),
    ("LeftSideSkirt", "Armor", "box", [pose(-3.6, -.6, -3.4, .8, .8, 2.5), pose(-3.1, 1.8, -.2, .8, 1.55, 1.35), pose(-3.45, 3.8, .4, .8, 1.75, 1.0)]),
    ("RightSideSkirt", "Armor", "box", [pose(3.6, -.6, -3.4, .8, .8, 2.5), pose(3.1, 1.8, -.2, .8, 1.55, 1.35), pose(3.45, 3.8, .4, .8, 1.75, 1.0)]),
    ("LeftKneeArmor", "Accent", "box", [pose(-2.5, -.7, -7.0, 1.2, .55, 1.5), pose(-2.2, .3, -1.2, 1.15, .9, .65), pose(-2.4, .4, 1.7, 1.25, 1.2, .5)]),
    ("RightKneeArmor", "Accent", "box", [pose(2.5, -.7, -7.0, 1.2, .55, 1.5), pose(2.2, .3, -1.2, 1.15, .9, .65), pose(2.4, .4, 1.7, 1.25, 1.2, .5)]),
    ("LeftCalfFin", "Armor", "box", [pose(-3.7, -.6, -7.5, .55, .55, 3.0, 0, 0, -.25), pose(-3.4, -1.5, -2.7, .5, 2.2, 1.2, 0, 0, -.18), pose(-3.75, -1.5, -.8, .45, 2.45, 1.4, 0, 0, -.16)]),
    ("RightCalfFin", "Armor", "box", [pose(3.7, -.6, -7.5, .55, .55, 3.0, 0, 0, .25), pose(3.4, -1.5, -2.7, .5, 2.2, 1.2, 0, 0, .18), pose(3.75, -1.5, -.8, .45, 2.45, 1.4, 0, 0, .16)]),
    ("LeftWingStripe", "Accent", "box", [pose(-7.0, .45, .8, 3.7, .18, .4, 0, 0, -.15), pose(-5.6, 4.8, -.2, 2.6, .2, .35, 0, 0, -.45), pose(-4.2, 6.3, -.2, .65, 1.55, .32, 0, 0, -.24)]),
    ("RightWingStripe", "Accent", "box", [pose(7.0, .45, .8, 3.7, .18, .4, 0, 0, .15), pose(5.6, 4.8, -.2, 2.6, .2, .35, 0, 0, .45), pose(4.2, 6.3, -.2, .65, 1.55, .32, 0, 0, .24)]),
    ("LeftAntenna", "Accent", "box", [pose(-.8, 2.2, 3.7, .18, 2.0, .18, 0, 0, -.12), pose(-.8, 9.0, 1.5, .18, 2.1, .18, 0, 0, -.12), pose(-.8, 12.4, -.1, .18, 2.3, .18, 0, 0, -.12)]),
    ("RightAntenna", "Accent", "box", [pose(.8, 2.2, 3.7, .18, 2.0, .18, 0, 0, .12), pose(.8, 9.0, 1.5, .18, 2.1, .18, 0, 0, .12), pose(.8, 12.4, -.1, .18, 2.3, .18, 0, 0, .12)]),
    ("LeftChestPlate", "Armor", "box", [pose(-1.9, 1.1, 4.1, 1.35, .9, 1.1, 0, 0, -.30), pose(-1.7, 6.0, 2.8, 1.3, 1.1, .55, 0, 0, -.30), pose(-1.6, 7.5, 2.2, 1.3, 1.35, .32, 0, 0, -.30)]),
    ("RightChestPlate", "Armor", "box", [pose(1.9, 1.1, 4.1, 1.35, .9, 1.1, 0, 0, .30), pose(1.7, 6.0, 2.8, 1.3, 1.1, .55, 0, 0, .30), pose(1.6, 7.5, 2.2, 1.3, 1.35, .32, 0, 0, .30)]),
    ("LeftHelmetCheek", "Armor", "box", [pose(-1.15, 1.0, 5.0, .25, .6, .55), pose(-1.15, 7.0, 2.7, .25, .65, .5), pose(-1.15, 10.0, .95, .25, .85, .42)]),
    ("RightHelmetCheek", "Armor", "box", [pose(1.15, 1.0, 5.0, .25, .6, .55), pose(1.15, 7.0, 2.7, .25, .65, .5), pose(1.15, 10.0, .95, .25, .85, .42)]),
    ("LeftToeArmor", "Armor", "box", [pose(-2.5, -.9, -10.7, 1.5, .4, 1.4), pose(-2.2, -4.5, 1.1, 1.55, .45, 1.25), pose(-2.4, -5.5, 3.5, 1.7, .45, 1.3)]),
    ("RightToeArmor", "Armor", "box", [pose(2.5, -.9, -10.7, 1.5, .4, 1.4), pose(2.2, -4.5, 1.1, 1.55, .45, 1.25), pose(2.4, -5.5, 3.5, 1.7, .45, 1.3)]),
    ("RifleBody", "Frame", "box", [pose(0, -.8, 8.0, .7, .7, 6.8), pose(-6.5, 1.0, 3.4, .7, 3.8, .7, 0, 0, -.22), pose(-7.2, -.4, 2.2, .7, 4.8, .7, 0, 0, -.18)]),
    ("RifleBarrel", "Accent", "box", [pose(0, -.8, 13.6, .26, .26, 1.6), pose(-7.6, -2.6, 3.4, .26, 1.5, .26, 0, 0, -.22), pose(-8.6, -5.0, 2.2, .26, 1.7, .26, 0, 0, -.18)]),
])


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for collection in list(bpy.data.collections):
        bpy.data.collections.remove(collection)


def make_materials():
    materials = {}
    for name, color in MATERIALS.items():
        material = bpy.data.materials.new(f"TF_{name}")
        material.diffuse_color = color
        material.use_nodes = True
        bsdf = next((node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED"), None)
        if bsdf is None:
            raise RuntimeError(f"Principled BSDF node was not created for {name}")
        bsdf.inputs["Base Color"].default_value = color
        bsdf.inputs["Metallic"].default_value = 0.72 if name != "Visor" else 0.25
        bsdf.inputs["Roughness"].default_value = 0.32 if name != "Visor" else 0.16
        if name == "Visor":
            bsdf.inputs["Emission Color"].default_value = color
            bsdf.inputs["Emission Strength"].default_value = 1.5
        materials[name] = material
    return materials


def set_pose_keyframe(obj, values, frame):
    location, scale, rotation = values
    # The game uses Y-up whereas Blender uses Z-up.  Convert positions, shape
    # axes, and local rotations so the Blender preview is upright.
    styled_location = tuple(value * factor for value, factor in zip(location, STYLE_TRANSLATE))
    styled_scale = tuple(value * factor for value, factor in zip(scale, STYLE_SCALE))
    obj.location = Vector((styled_location[0], styled_location[2], styled_location[1])) * MODEL_SCALE
    obj.scale = Vector((styled_scale[0], styled_scale[2], styled_scale[1])) * MODEL_SCALE
    axis_swap = Matrix(((1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0)))
    game_rotation = Euler(rotation, "XYZ").to_matrix()
    obj.rotation_euler = (axis_swap @ game_rotation @ axis_swap).to_euler("XYZ")
    obj.keyframe_insert(data_path="location", frame=frame)
    obj.keyframe_insert(data_path="scale", frame=frame)
    obj.keyframe_insert(data_path="rotation_euler", frame=frame)


def add_part(name, material, primitive, poses, collection):
    if primitive == "cylinder":
        bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=1.0, depth=2.0)
    else:
        bpy.ops.mesh.primitive_cube_add()
    obj = bpy.context.active_object
    obj.name = name
    obj.data.materials.append(material)
    for child_collection in list(obj.users_collection):
        child_collection.objects.unlink(obj)
    collection.objects.link(obj)
    obj.rotation_mode = "XYZ"
    if primitive == "box":
        bevel = obj.modifiers.new("Armor edge bevel", "BEVEL")
        bevel.width = 0.10
        bevel.segments = 2
        bevel.limit_method = "ANGLE"
    for frame, pose_values in zip(FRAMES, poses):
        set_pose_keyframe(obj, pose_values, frame)
    return obj


def set_smooth_animation():
    for obj in bpy.context.scene.objects:
        if obj.animation_data and obj.animation_data.action:
            for curve in obj.animation_data.action.fcurves:
                for point in curve.keyframe_points:
                    point.interpolation = "BEZIER"


def add_scene_helpers(collection):
    # Ground grid offers a size reference in the Blender viewport.
    bpy.ops.mesh.primitive_plane_add(size=6.0, location=(0, 0, -0.48))
    floor = bpy.context.active_object
    floor.name = "Preview_Ground"
    floor_material = bpy.data.materials.new("PreviewGround")
    floor_material.diffuse_color = (0.045, 0.055, 0.075, 1.0)
    floor.data.materials.append(floor_material)

    preview_target = Vector((0.0, 0.0, 0.35))

    bpy.ops.object.light_add(type="AREA", location=(3.5, 4.0, 5.0))
    key = bpy.context.active_object
    key.name = "Preview_KeyLight"
    key.data.energy = 900
    key.data.shape = "DISK"
    key.data.size = 4.0
    key.rotation_euler = (preview_target - key.location).to_track_quat("-Z", "Y").to_euler()

    bpy.ops.object.light_add(type="AREA", location=(-3.5, -2.5, 3.0))
    fill = bpy.context.active_object
    fill.name = "Preview_FillLight"
    fill.data.energy = 500
    fill.data.size = 3.0
    fill.rotation_euler = (preview_target - fill.location).to_track_quat("-Z", "Y").to_euler()

    bpy.ops.object.camera_add(location=(2.4, 4.0, 1.9))
    camera = bpy.context.active_object
    camera.name = "Preview_Camera"
    camera.rotation_euler = (preview_target - camera.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = camera


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    clear_scene()
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 960
    scene.render.resolution_y = 960
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(PREVIEW_PATH)
    scene.world.color = (0.025, 0.035, 0.055)
    scene.frame_start, scene.frame_end = FRAMES[0], FRAMES[-1]

    collection = bpy.data.collections.new("TransformFighter")
    scene.collection.children.link(collection)
    materials = make_materials()
    for name, material_name, primitive, poses in PARTS:
        add_part(name, materials[material_name], primitive, poses, collection)
    set_smooth_animation()

    for frame, marker_name in zip(FRAMES, ("Fighter", "Gerwalk", "Battroid")):
        scene.timeline_markers.new(marker_name, frame=frame)
    add_scene_helpers(collection)
    scene.frame_set(FRAMES[-1])
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    bpy.ops.export_scene.gltf(filepath=str(GLB_PATH), export_format="GLB", export_animations=True)
    bpy.ops.render.render(write_still=True)
    print(f"Created: {BLEND_PATH}")
    print(f"Created: {GLB_PATH}")
    print(f"Created: {PREVIEW_PATH}")


if __name__ == "__main__":
    main()
