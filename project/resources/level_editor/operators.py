import json
import math
import os

import bpy
import bpy_extras


def _default_scene_json_path():
    blend_dir = os.path.dirname(bpy.data.filepath)
    if not blend_dir:
        return "scene.json"
    return os.path.join(blend_dir, "scene.json")


class MYADDON_OT_stretch_vertex(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_stretch_vertex"
    bl_label = "頂点を伸ばす"
    bl_description = "頂点座標を引っ張って伸ばします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.data.objects["Cube"].data.vertices[0].co.x += 1.0
        print("頂点を伸ばしました。")
        return {'FINISHED'}


class MYADDON_OT_create_ico_sphere(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_object"
    bl_label = "ICO球生成"
    bl_description = "ICO球を生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_ico_sphere_add()
        print("ICO球を生成しました。")
        return {'FINISHED'}


class MYADDON_OT_export_scene(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    bl_idname = "myaddon.myaddon_ot_export_scene"
    bl_label = "シーン出力"
    bl_description = "シーン情報をゲーム用scene.jsonへ出力します"

    filename_ext = ".json"

    def _build_object_data(self, obj):
        trans, rot, scale = obj.matrix_local.decompose()
        rot = rot.to_euler()
        rot.x, rot.y, rot.z = [math.degrees(v) for v in rot]

        export_scale = scale
        if obj.name.startswith("StageBounds"):
            dimensions = obj.dimensions
            export_scale = (
                dimensions.x * 0.5,
                dimensions.y * 0.5,
                dimensions.z * 0.5,
            )

        obj_data = {
            "type": obj.type,
            "name": obj.name,
            "transform": {
                "translation": [trans.x, trans.y, trans.z],
                "rotation": [rot.x, rot.y, rot.z],
                "scale": [export_scale[0], export_scale[1], export_scale[2]],
            },
        }

        if hasattr(obj, "collider") and obj.collider != "None":
            if obj.collider in ["Box", "BOX"]:
                center = obj.collider_center
                size = obj.collider_size
                obj_data["collider"] = {
                    "type": obj.collider,
                    "center": [center[0], center[1], center[2]],
                    "size": [size[0], size[1], size[2]],
                }

        if hasattr(obj, "game_obj_type") and obj.game_obj_type != 'NONE':
            obj_data["category"] = obj.game_obj_type

        if hasattr(obj, "enemy_type") and obj.enemy_type != "None":
            obj_data["enemy"] = {
                "type": obj.enemy_type,
            }

        if obj.type == 'MESH':
            mesh = obj.to_mesh()
            obj_data["vertices_count"] = len(mesh.vertices)
            obj.to_mesh_clear()
        else:
            obj_data["vertices_count"] = 0

        return obj_data

    def export(self):
        if not self.filepath:
            self.filepath = _default_scene_json_path()

        scene_data = {
            "name": "scene",
            "objects": [],
        }

        for obj in bpy.context.scene.objects:
            if obj.type in ['MESH', 'EMPTY']:
                scene_data["objects"].append(self._build_object_data(obj))

        with open(self.filepath, "wt", encoding="utf-8") as file:
            json.dump(scene_data, file, ensure_ascii=False, indent=4)

    def invoke(self, context, event):
        self.filepath = _default_scene_json_path()
        return self.execute(context)

    def execute(self, context):
        if not self.filepath:
            self.filepath = _default_scene_json_path()
        self.export()
        self.report({'INFO'}, f"シーン情報をExportしました: {self.filepath}")
        return {'FINISHED'}


class MYADDON_OT_create_stage_bounds(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_stage_bounds"
    bl_label = "ステージ範囲を配置"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_cube_add(size=2.0)
        obj = context.active_object
        obj.name = "StageBounds"
        obj.scale = (10.0, 10.0, 10.0)
        obj.game_obj_type = 'OBSTACLE'
        obj.display_type = 'WIRE'
        print("ワイヤーフレームのステージ範囲を配置しました。")
        return {'FINISHED'}


class MYADDON_OT_create_spawn_point(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_spawn_point"
    bl_label = "スポーン配置"
    bl_options = {'REGISTER', 'UNDO'}

    spawn_type: bpy.props.StringProperty(default="PLAYER")

    def execute(self, context):
        bpy.ops.object.empty_add(type='SINGLE_ARROW', radius=2.0)
        obj = context.active_object

        if self.spawn_type == "PLAYER":
            obj.name = "PlayerSpawn"
            obj.game_obj_type = 'PLAYER'
            print("自機のスポーン地点を配置しました。")
        else:
            obj.name = "EnemySpawn"
            obj.game_obj_type = 'ENEMY'
            print("敵のスポーン地点を配置しました。")

        return {'FINISHED'}


classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
    MYADDON_OT_create_stage_bounds,
    MYADDON_OT_create_spawn_point,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
