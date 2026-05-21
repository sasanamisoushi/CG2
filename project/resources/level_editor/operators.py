import bpy
import bpy_extras
import math
import json

# 頂点を伸ばす
class MYADDON_OT_stretch_vertex(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_stretch_vertex"
    bl_label = "頂点を伸ばす"
    bl_description = "頂点座標を引っ張って伸ばします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.data.objects["Cube"].data.vertices[0].co.x += 1.0
        print("頂点を伸ばしました。")
        return {'FINISHED'}

# ICO球生成
class MYADDON_OT_create_ico_sphere(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_object"
    bl_label = "ICO球生成"
    bl_description = "ICO球を生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_ico_sphere_add()
        print("ICO球を生成しました。")
        return {'FINISHED'}

# シーン出力
class MYADDON_OT_export_scene(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    bl_idname = "myaddon.myaddon_ot_export_scene"
    bl_label = "シーン出力"
    bl_description = "シーン情報をExportします"

    filename_ext = ".json"

    def write_and_print(self, file, str):
        print(str)
        file.write(str)
        file.write('\n')

    def parse_mesh_data(self, file, object):
        mesh = object.to_mesh()
        self.write_and_print(file, "MESH_DATA")
        self.write_and_print(file, "Vertices: %d" % len(mesh.vertices))
        object.to_mesh_clear()

    def export(self):
       # 1. 保存するデータをすべてまとめるための大きな「辞書」を用意
        scene_data = {
            "name": "scene",
            "objects": []
        }

        for object in bpy.context.scene.objects:
           # ===================================================
            # MESH（モデル）または EMPTY（ブランク）の両方を許可する！
            # ===================================================
            if object.type in ['MESH', 'EMPTY']:
                # オブジェクト1つ分のデータを入れる辞書
                obj_data = {
                    "type": object.type,
                    "name": object.name,
                }
                
                # トランスフォームの計算
                trans, rot, scale = object.matrix_local.decompose()
                rot = rot.to_euler()
                rot.x, rot.y, rot.z = [math.degrees(v) for v in rot]
                
                # トランスフォーム情報を辞書に追加
                obj_data["transform"] = {
                    "translation": [trans.x, trans.y, trans.z],
                    "rotation": [rot.x, rot.y, rot.z],
                    "scale": [scale.x, scale.y, scale.z]
                }
                
                # Boxコライダーのデータを辞書に追加
                if hasattr(object, "collider") and object.collider != "None":
                    if object.collider in ["Box", "BOX"]:
                        center = object.collider_center
                        size = object.collider_size
                        obj_data["collider"] = {
                            "type": object.collider,
                            "center": [center[0], center[1], center[2]],
                            "size": [size[0], size[1], size[2]]
                        }

                # オブジェクトの種類があればJSONに書き出す！
                if hasattr(object, "game_obj_type") and object.game_obj_type != 'NONE':
                    obj_data["category"] = object.game_obj_type        

                # ★敵のデータがあればJSONに書き出す（先ほど追加したもの）
                if hasattr(object, "enemy_type") and object.enemy_type != "None":
                    obj_data["enemy"] = {
                        "type": object.enemy_type
                    }

                # ===================================================
                # MESHのときだけ頂点数を数える（ブランクでやるとエラーになるため）
                # ===================================================
                if object.type == 'MESH':
                    mesh = object.to_mesh()
                    obj_data["vertices_count"] = len(mesh.vertices)
                    object.to_mesh_clear()
                else:
                    obj_data["vertices_count"] = 0

                # 完成したオブジェクトデータをリストに追加
                scene_data["objects"].append(obj_data)

        # 2. JSONファイルとして書き出し
        with open(self.filepath, "wt") as file:
            # json.dumpを使って、辞書データをきれいにフォーマットして書き込む
            json.dump(scene_data, file, indent=4)

    def execute(self, context):
        self.export()
        self.report({'INFO'}, "シーン情報をExportしました")
        return {'FINISHED'}

classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
