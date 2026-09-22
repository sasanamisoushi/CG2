"""Create a Blender review file for the vf-15c left-arm guard pose.

The game model is made of rigid mesh nodes rather than a conventional armature.
These helper empties therefore behave as the shoulder, elbow, and wrist bones
used by the in-game guard code.  The rifle is parented to the left hand, so
the guard animation moves the guarding left arm, including its hand-mounted
rifle and the detached guard panel.
"""

from pathlib import Path
import json
import math
import struct

import bpy
from mathutils import Matrix, Quaternion, Vector


PROJECT_DIR = Path(__file__).resolve().parent.parent
SOURCE = PROJECT_DIR / "resources" / "vf-15c" / "scene.gltf"
IMPORT_SOURCE = PROJECT_DIR / "resources" / "vf-15c" / "scene_for_blender_import.glb"
OUTPUT = PROJECT_DIR / "resources" / "vf-15c" / "vf15c_battroid_left_guard_preview.blend"
RENDER = PROJECT_DIR / "resources" / "vf-15c" / "vf15c_battroid_left_guard_preview.png"


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for item in list(datablocks):
            datablocks.remove(item)


def load_battroid_gltf_animation_pose(path, battroid_time):
    """Read the 5-second Battroid key of the embedded glTF animation.

    Blender does not import the animation from this particular GLB-formatted
    `.gltf` file reliably.  Reading the standard glTF accessors directly keeps
    this review file aligned with the same 5-second Battroid state used by the
    game.
    """
    data = path.read_bytes()
    if data[:4] != b"glTF":
        raise RuntimeError("vf-15c source is expected to be a binary glTF file")

    offset = 12
    json_chunk = None
    binary_chunk = None
    while offset < len(data):
        length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset:offset + length]
        offset += length
        if chunk_type == 0x4E4F534A:  # JSON
            json_chunk = chunk
        elif chunk_type == 0x004E4942:  # BIN\0
            binary_chunk = chunk

    if json_chunk is None or binary_chunk is None:
        raise RuntimeError("vf-15c source has no readable glTF animation data")
    gltf = json.loads(json_chunk.decode("utf-8"))
    animation = gltf["animations"][0]
    component_counts = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}

    def read_accessor(accessor_index):
        accessor = gltf["accessors"][accessor_index]
        if accessor.get("componentType") != 5126:
            raise RuntimeError("vf-15c animation uses an unsupported accessor type")
        count = accessor["count"]
        component_count = component_counts[accessor["type"]]
        view = gltf["bufferViews"][accessor["bufferView"]]
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        stride = view.get("byteStride", component_count * 4)
        return [
            struct.unpack_from("<" + "f" * component_count, binary_chunk, start + item * stride)
            for item in range(count)
        ]

    pose = {}
    for channel in animation["channels"]:
        sampler = animation["samplers"][channel["sampler"]]
        times = read_accessor(sampler["input"])
        values = read_accessor(sampler["output"])
        key_index = min(range(len(times)), key=lambda index: abs(times[index][0] - battroid_time))
        node_name = gltf["nodes"][channel["target"]["node"]].get("name")
        if node_name:
            pose.setdefault(node_name, {})[channel["target"]["path"]] = values[key_index]
    return pose


def apply_battroid_pose(pose):
    # glTF is Y-up while the imported Blender scene is Z-up.
    # The importer maps (x, y, z) to (x, -z, y), so the sampled animation
    # must receive that identical conversion before it is assigned.
    coordinate_rotation = Matrix.Rotation(math.radians(90.0), 4, "X").to_quaternion()
    for node_name, transform in pose.items():
        obj = bpy.data.objects.get(node_name)
        if obj is None:
            continue
        if "translation" in transform:
            x, y, z = transform["translation"]
            obj.location = (x, -z, y)
        if "rotation" in transform:
            x, y, z, w = transform["rotation"]
            obj.rotation_mode = "QUATERNION"
            source_rotation = Quaternion((w, x, y, z))
            obj.rotation_quaternion = coordinate_rotation @ source_rotation @ coordinate_rotation.conjugated()
        if "scale" in transform:
            x, y, z = transform["scale"]
            obj.scale = (x, z, y)
    bpy.context.view_layer.update()


def get_object(name):
    result = bpy.data.objects.get(name)
    if result is None:
        raise RuntimeError(f"Required vf-15c node was not imported: {name}")
    return result


def parent_keep_world(child, parent):
    # Newly-created helpers have not been evaluated yet.  Force evaluation
    # before taking the inverse; otherwise Blender returns an identity matrix
    # and applies the helper translation a second time.
    bpy.context.view_layer.update()
    child.parent = parent
    bpy.context.view_layer.update()
    # Assigning a parent alone makes Blender interpret the former world
    # translation as a local translation, which doubled the arm offset and
    # was the direct cause of the floating parts in the prior preview.
    child.matrix_parent_inverse = parent.matrix_world.inverted()


def create_bone_helper(name, world_position, parent=None):
    helper = bpy.data.objects.new(name, None)
    helper.empty_display_type = "SPHERE"
    helper.empty_display_size = 0.18
    helper.color = (0.2, 0.8, 1.0, 1.0)
    bpy.context.collection.objects.link(helper)
    helper.location = world_position
    if parent is not None:
        parent_keep_world(helper, parent)
    return helper


def key_transform(obj, frame):
    obj.keyframe_insert(data_path="location", frame=frame)
    obj.keyframe_insert(data_path="rotation_euler", frame=frame)


def add_review_lighting(center, radius):
    bpy.ops.object.light_add(type="AREA", location=center + Vector((radius, -radius, radius * 1.5)))
    key = bpy.context.object
    key.data.energy = 1300
    key.data.shape = "DISK"
    key.data.size = radius * 1.5

    bpy.ops.object.light_add(type="AREA", location=center + Vector((-radius, radius * 0.5, radius)))
    fill = bpy.context.object
    fill.data.energy = 800
    fill.data.size = radius


def point_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def add_review_camera(center, radius):
    bpy.ops.object.camera_add(location=center + Vector((radius * 2.4, -radius * 4.8, radius * 1.8)))
    camera = bpy.context.object
    camera.name = "GuardReviewCamera"
    camera.data.lens = 45
    point_at(camera, center + Vector((0.0, 0.0, radius * 0.1)))
    bpy.context.scene.camera = camera


def model_bounds():
    mesh_objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    points = [obj.matrix_world @ Vector(corner) for obj in mesh_objects for corner in obj.bound_box]
    minimum = Vector((min(point.x for point in points), min(point.y for point in points), min(point.z for point in points)))
    maximum = Vector((max(point.x for point in points), max(point.y for point in points), max(point.z for point in points)))
    center = (minimum + maximum) * 0.5
    radius = max(maximum - minimum) * 0.55
    return center, max(radius, 1.0)


clear_scene()
# The game asset contains a binary GLB payload despite its .gltf extension.
# Import its rest geometry, then apply the sampled transform using the same
# glTF-to-Blender coordinate conversion as the importer.
IMPORT_SOURCE.write_bytes(SOURCE.read_bytes())
bpy.ops.import_scene.gltf(filepath=str(IMPORT_SOURCE))
apply_battroid_pose(load_battroid_gltf_animation_pose(SOURCE, 3.5))
for obj in bpy.context.scene.objects:
    obj.animation_data_clear()

# In the game coordinate convention, the unsuffixed side is the visible left arm.
shoulder = get_object("Shoulder_29")
shoulder_joint = get_object("ShoulderJoint1_28")
upper_arm = get_object("UpperArm_30")
elbow = get_object("Elbow_21")
forearm = get_object("Forearm_22")
hand = get_object("Hand_23")
right_hand = get_object("Hand.001_46")
gun_grip = get_object("GunGrip_100")
barrel_back = get_object("BarrelBack_98")
barrel_front = get_object("barrelFront_99")
head_roots = (
    get_object("Cylinder.005_80"),
    get_object("Cylinder.006_81"),
    get_object("Cylinder.007_82"),
    get_object("Cylinder.009_85"),
    get_object("Cylinder.013_83"),
)

def set_world_position(obj, position):
    matrix = obj.matrix_world.copy()
    matrix.translation = Vector(position)
    obj.matrix_world = matrix


def part_meshes(part):
    """Return all meshes belonging to an imported glTF node."""
    return [child for child in part.children_recursive if child.type == "MESH"]


def part_center(part):
    """Get the center of the visible geometry, not the glTF node origin.

    The vf-15c glTF has several limb meshes whose vertices are offset far away
    from their node origins.  Positioning the origins leaves an arm visibly
    floating even if its numeric node location looks correct.
    """
    points = [
        mesh.matrix_world @ Vector(corner)
        for mesh in part_meshes(part)
        for corner in mesh.bound_box
    ]
    if not points:
        return part.matrix_world.translation.copy()
    return sum(points, Vector()) / len(points)


def set_part_center(part, position):
    """Move a rigid glTF node until its rendered mesh center reaches position."""
    delta = Vector(position) - part_center(part)
    set_world_position(part, part.matrix_world.translation + delta)
    bpy.context.view_layer.update()


def group_center(parts):
    points = [
        mesh.matrix_world @ Vector(corner)
        for part in parts
        for mesh in part_meshes(part)
        for corner in mesh.bound_box
    ]
    return sum(points, Vector()) / len(points)


# Keep the source's fully transformed Battroid arrangement intact.  Its arm
# mesh centers are already aligned to the shoulder armor; manually moving node
# origins was what caused the two arms to look detached in the prior preview.
# The imported weapon is stored beside the mirrored arm, even though the
# requested guard pose holds it on the visible, screen-right guard hand.
weapon_parts = (gun_grip, barrel_back, barrel_front)
# Cube.040_61 owns Object_128, the panel selected in the review.  It was never
# included in the arm hierarchy, which is why it remained behind in the air.
guard_panel = get_object("Cube.040_61")
# These are the separate rigid hand/forearm armor nodes surrounding Hand_23 in
# the imported asset.  They are not children of the arm joints in the source
# file, so explicitly include them in the guard wrist hierarchy.
left_hand_attachments = tuple(get_object(name) for name in (
    "Cube.021_75", "Cube.017_74", "Cube.039_78", "Cube.013_51",
    "Cube.018_56", "Cube.015_55", "Cube.022_57", "Cube.001_52",
    "Cube.042_79", "Cube.020_66", "Cube.041_70", "Cube.006_3",
    "Cube.016_65", "Cube.038_69", "Cube.012_50", "Cube.019_95",
))
# The head nodes use the fighter's rear-facing transform in this source file.
# Put their visible center at the top-front of the Battroid torso so Blender's
# front view shows the face rather than the rear head shell.
head_delta = Vector((0.0, -1.35, 17.35)) - group_center(head_roots)
for part in head_roots:
    set_world_position(part, part.matrix_world.translation + head_delta)
    bpy.context.view_layer.update()
# The head mesh was facing the rear in the original transform data.  Rotate
# all its rigid components as one assembly around their common center.
head_facing = create_bone_helper("GuardBone_HeadFacing", group_center(head_roots))
for part in head_roots:
    parent_keep_world(part, head_facing)
head_facing.rotation_euler.z = math.pi
bpy.context.view_layer.update()

# The rifle is held by the right hand.  Move its grip to that hand once, then
# parent all three weapon roots there so Object_190 and every barrel mesh stay
# attached while the left arm performs the guard motion.
weapon_delta = part_center(right_hand) - part_center(gun_grip)
for part in weapon_parts:
    set_world_position(part, part.matrix_world.translation + weapon_delta)
    parent_keep_world(part, right_hand)
    bpy.context.view_layer.update()
shoulder_bone = create_bone_helper("GuardBone_LeftShoulder", part_center(shoulder))
elbow_bone = create_bone_helper("GuardBone_LeftElbow", part_center(elbow), shoulder_bone)
wrist_bone = create_bone_helper("GuardBone_LeftWrist", part_center(hand), elbow_bone)

for obj in (shoulder, shoulder_joint, upper_arm):
    parent_keep_world(obj, shoulder_bone)
for obj in (elbow, forearm):
    parent_keep_world(obj, elbow_bone)
for obj in (hand, guard_panel) + left_hand_attachments:
    parent_keep_world(obj, wrist_bone)

scene = bpy.context.scene
scene.frame_start = 1
scene.frame_end = 18
scene.timeline_markers.new("Battroid_Rest", frame=1)
# The user-approved reference pose was checked at frame 14, so make that the
# completed guard pose and leave frames 15-18 as a hold.
scene.timeline_markers.new("Battroid_Guard_Left", frame=14)

# Frame 1: rest pose.
scene.frame_set(1)
key_transform(shoulder_bone, 1)
key_transform(elbow_bone, 1)
key_transform(wrist_bone, 1)

# Frame 14: a Battroid guard inspired by Macross Delta Scramble: bring the
# left upper arm forward/inward, bend the elbow sharply, and hold the forearm
# upright in front of the cockpit.  The rifle and guard panel follow the hand.
scene.frame_set(14)

# Front view is -Y.  The shoulder pulls the whole arm forward and inward,
# then the elbow folds the forearm toward the chest.  The hand, rifle roots,
# and the selected guard panel are children of wrist_bone.
shoulder_bone.rotation_euler.x = -0.55
shoulder_bone.rotation_euler.y = 0.20
shoulder_bone.rotation_euler.z = 0.16
elbow_bone.rotation_euler.x = -1.30
wrist_bone.rotation_euler.x = 0.12
key_transform(shoulder_bone, 14)
key_transform(elbow_bone, 14)
key_transform(wrist_bone, 14)

# Keep the completed pose through the end of the review range.
scene.frame_set(18)
key_transform(shoulder_bone, 18)
key_transform(elbow_bone, 18)
key_transform(wrist_bone, 18)

for helper in (shoulder_bone, elbow_bone, wrist_bone):
    if helper.animation_data and helper.animation_data.action:
        for curve in helper.animation_data.action.fcurves:
            for point in curve.keyframe_points:
                point.interpolation = "BEZIER"

scene.frame_set(14)
center, radius = model_bounds()
add_review_lighting(center, radius)
add_review_camera(center, radius)
scene.render.engine = "BLENDER_EEVEE_NEXT"
scene.render.resolution_x = 1280
scene.render.resolution_y = 720
scene.render.resolution_percentage = 100
scene.world.color = (0.035, 0.035, 0.05)
scene.render.filepath = str(RENDER)
bpy.ops.render.render(write_still=True)

bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT))
print(f"Saved guard review file: {OUTPUT}")
