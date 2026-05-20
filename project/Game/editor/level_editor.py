import bpy
import math
import mathutils
import bpy_extras
import gpu
from gpu_extras.batch import batch_for_shader
import copy
import json

# ブレンダーに登録するアドオン情報
bl_info = {
    "name": "レベルエディタ",
    "author": "Taro Kamata",
    "version": (1, 0),
    "blender": (3, 3, 1),
    "location": "",
    "description": "レベルエディタ",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Object"
}

# ---------------------------------------------------------
# コライダー描画クラス 
# ---------------------------------------------------------
class DrawCollider:
    handle = None

    @staticmethod
    def draw_collider():
        # --- 資料スライド1：頂点データの初期化（空にしておく） ---
        vertices = {"pos": []}
        indices = []
        
        # --- 資料スライド2：頂点オフセットとサイズの用意 ---
        offsets = [
            [-0.5, -0.5, -0.5], # 左下前
            [+0.5, -0.5, -0.5], # 右下前
            [-0.5, +0.5, -0.5], # 左上前
            [+0.5, +0.5, -0.5], # 右上前
            [-0.5, -0.5, +0.5], # 左下奥
            [+0.5, -0.5, +0.5], # 右下奥
            [-0.5, +0.5, +0.5], # 左上奥
            [+0.5, +0.5, +0.5], # 右上奥
        ]

        # --- 資料スライド3・4：全オブジェクト走査 ---
        for object in bpy.context.scene.objects:
            
            # colliderプロパティがBoxのオブジェクトだけ処理する
            if hasattr(object, "collider") and object.collider in ["Box", "BOX"]:

                # UIパネルに入力されている「中心」と「サイズ」のデータを取得
                center = object.collider_center
                size = object.collider_size
                
                # ① 追加前の頂点数（開始インデックス）を記録
                start = len(vertices["pos"])
                
                # Boxの8頂点分回す
                for offset in offsets:
                   # ① まず、UIで設定したコライダーの「サイズ」と「中心」を使って、
                    # オブジェクトから見たローカルな頂点座標(local_pos)を計算する
                    local_pos = mathutils.Vector((
                        offset[0] * size[0] + center[0],
                        offset[1] * size[1] + center[1],
                        offset[2] * size[2] + center[2]
                    ))
                    
                    # ② そのローカル座標に、オブジェクトのトランスフォーム行列（位置・回転・スケール）を掛け合わせて、
                    # 最終的なワールド空間での座標(world_pos)に変換する
                    world_pos = object.matrix_world @ local_pos
                    
                    # 頂点データリストに座標を追加
                    vertices['pos'].append(world_pos)
                
                # 前面を構成する辺の頂点インデックス
                indices.append([start+0, start+1])
                indices.append([start+2, start+3])
                indices.append([start+0, start+2])
                indices.append([start+1, start+3])
                # 奥面を構成する辺の頂点インデックス
                indices.append([start+4, start+5])
                indices.append([start+6, start+7])
                indices.append([start+4, start+6])
                indices.append([start+5, start+7])
                # 手前と奥を繋ぐ辺の頂点インデックス
                indices.append([start+0, start+4])
                indices.append([start+1, start+5])
                indices.append([start+2, start+6])
                indices.append([start+3, start+7])
        
        # 万が一、描画するBoxが一つもなかったらここで処理を終わる（エラー防止）
        if len(vertices["pos"]) == 0:
            return

        # --- 最後に1回だけ描画処理を行う（for文の外に出す） ---
        shader = gpu.shader.from_builtin('UNIFORM_COLOR')
        # verticesは既に辞書型 {"pos": [...]} になっているのでそのまま渡す
        batch = batch_for_shader(shader, 'LINES', vertices, indices=indices)
        
        gpu.state.depth_test_set('ALWAYS') 
        gpu.state.line_width_set(2.0)
        
        shader.bind()
        shader.uniform_float("color", (0.0, 1.0, 1.0, 1.0)) # 水色
        batch.draw(shader)
        
        # 設定を元に戻す
        gpu.state.depth_test_set('LESS')
        gpu.state.line_width_set(1.0)
# ---------------------------------------------------------
# オペレータ類
# ---------------------------------------------------------

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
            # indent=4 を付けることで、人間が見やすいように改行と空白が入ります
            json.dump(scene_data, file, indent=4)

    def execute(self, context):
        self.export()
        self.report({'INFO'}, "シーン情報をExportしました")
        return {'FINISHED'}

# ---------------------------------------------------------
# メニュー類
# ---------------------------------------------------------

class TOPBAR_MT_my_menu(bpy.types.Menu):
    bl_idname = "myaddon.topbar_mt_my_menu"
    bl_label = "MyMenu"

    def draw(self, context):
        self.layout.operator(MYADDON_OT_stretch_vertex.bl_idname, text=MYADDON_OT_stretch_vertex.bl_label)
        self.layout.operator(MYADDON_OT_create_ico_sphere.bl_idname, text=MYADDON_OT_create_ico_sphere.bl_label)
        self.layout.operator(MYADDON_OT_export_scene.bl_idname, text=MYADDON_OT_export_scene.bl_label)

    def submenu(self, context):
        self.layout.menu(TOPBAR_MT_my_menu.bl_idname)

# ---------------------------------------------------------
# パネル類 (ここが今回の追加ポイント)
# ---------------------------------------------------------

class OBJECT_PT_file_name(bpy.types.Panel):
    """オブジェクトのファイル名パネル"""
    bl_idname = "OBJECT_PT_file_name"
    bl_label = "FileName"
    bl_space_type = 'PROPERTIES'   # プロパティウィンドウに表示
    bl_region_type = 'WINDOW'      # ウィンドウ内に配置
    bl_context = "object"          # 「オブジェクト」タブに表示

    def draw(self, context):
        layout = self.layout
        obj = context.object
        # スライド通りの内容
        layout.label(text="Hello")
        layout.separator()
        layout.label(text="Hello2", icon='MESH_CUBE')
        # シーンのプロパティをUIに表示する
        layout.prop(context.scene, "myaddon_custom_string", text="カスタム文字列")
        # Boxコライダーのメニューを実装
        box = layout.box()
        box.label(text="Collider")
        box.prop(obj, "collider", text="タイプ")
        box.prop(obj, "collider_center", text="中心")
        box.prop(obj, "collider_size", text="サイズ")
        # 敵の設定パネル
        enemy_box = layout.box()
        enemy_box.label(text="Enemy Settings")
        enemy_box.prop(obj, "enemy_type", text="敵の種類")
        # エクスポート用の区切り線、ラベル、実行ボタン
        layout.separator()
        layout.label(text="Export")
        layout.operator(MYADDON_OT_export_scene.bl_idname, text="Scene Export")


import socket

# =========================================================
# リアルタイム通信（UDP）の処理を追加！
# =========================================================
UDP_IP = "127.0.0.1" # 自分自身のPC宛て
UDP_PORT = 50000     # C++で指定した50000番ポート

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send_scene_realtime(scene):
    # エクスポートと同じようにデータを作る
    scene_data = {"objects": []}
    for object in bpy.context.scene.objects:
        if object.type in ['MESH', 'EMPTY']:
            obj_data = {"type": object.type, "name": object.name}
            trans, rot, scale = object.matrix_local.decompose()
            obj_data["transform"] = {
                "translation": [trans.x, trans.y, trans.z],
                "scale": [scale.x, scale.y, scale.z]
            }
            if hasattr(object, "enemy_type") and object.enemy_type != "None":
                obj_data["enemy"] = {"type": object.enemy_type}
            scene_data["objects"].append(obj_data)

    # JSONにしてC++エンジンへ投げつける！
    json_str = json.dumps(scene_data)
    try:
        sock.sendto(json_str.encode('utf-8'), (UDP_IP, UDP_PORT))
    except:
        pass # もしC++側が起動していなくてもエラーを出さない

# Blenderの画面内で「何かが更新されたとき」に自動で上の関数を呼ぶ設定
def register_realtime_sync():
    if send_scene_realtime not in bpy.app.handlers.depsgraph_update_post:
        bpy.app.handlers.depsgraph_update_post.append(send_scene_realtime)

# 実行した瞬間に監視スタート
register_realtime_sync()

# ---------------------------------------------------------
# 登録・抹消
# ---------------------------------------------------------

classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
    TOPBAR_MT_my_menu,
    OBJECT_PT_file_name, # クラスリストに追加
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_editor_menus.append(TOPBAR_MT_my_menu.submenu)
    
    # プロパティ登録
    bpy.types.Scene.myaddon_custom_string = bpy.props.StringProperty(name="カスタム文字列")
    bpy.types.Object.base_name = bpy.props.StringProperty(name="ベース名", default="None")

    bpy.types.Object.collider = bpy.props.StringProperty(name="タイプ", default="None")
    bpy.types.Object.collider_center = bpy.props.FloatVectorProperty(name="中心", size=3, default=(0.0, 0.0, 0.0))
    bpy.types.Object.collider_size = bpy.props.FloatVectorProperty(name="サイズ", size=3, default=(2.0, 2.0, 2.0))
    bpy.types.Object.enemy_type = bpy.props.StringProperty(name="敵タイプ", default="None")

    # ★コライダー描画関数の登録 (Slide 01_09)
    DrawCollider.handle = bpy.types.SpaceView3D.draw_handler_add(
        DrawCollider.draw_collider, (), 'WINDOW', 'POST_VIEW'
    )
    print("レベルエディタが有効化されました。")

def unregister():
    # ★コライダー描画関数の削除 (Slide 01_09)
    if DrawCollider.handle:
        bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, 'WINDOW')
        DrawCollider.handle = None

    bpy.types.TOPBAR_MT_editor_menus.remove(TOPBAR_MT_my_menu.submenu)
    for cls in classes:
        bpy.utils.unregister_class(cls)
    
    del bpy.types.Scene.myaddon_custom_string
    del bpy.types.Object.base_name
    print("レベルエディタが無効化されました。")

if __name__ == "__main__":
    register()