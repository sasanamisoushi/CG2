import bpy
import operators

class TOPBAR_MT_my_menu(bpy.types.Menu):
    bl_idname = "myaddon.topbar_mt_my_menu"
    bl_label = "MyMenu"

    def draw(self, context):
        self.layout.operator(operators.MYADDON_OT_stretch_vertex.bl_idname, text=operators.MYADDON_OT_stretch_vertex.bl_label)
        self.layout.operator(operators.MYADDON_OT_create_ico_sphere.bl_idname, text=operators.MYADDON_OT_create_ico_sphere.bl_label)
        self.layout.operator(operators.MYADDON_OT_export_scene.bl_idname, text=operators.MYADDON_OT_export_scene.bl_label)

    def submenu(self, context):
        self.layout.menu(TOPBAR_MT_my_menu.bl_idname)


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

        # ゲーム用設定パネルを作る
        game_box = layout.box()
        game_box.label(text="Game Settings")
        game_box.prop(obj, "game_obj_type", text="種類")

        # 敵が選ばれた時だけ、敵のタイプを入力させる！
        if obj.game_obj_type == 'ENEMY':
            game_box.prop(obj, "enemy_type", text="敵のタイプ")

        # ★修正1：区切り線のインデント（字下げ）を左に戻しました！
        layout.separator()
        layout.label(text="=== レベル構築ツール ===")
        
        # ★修正2：アイコンを 'WIRE' から 'SHADING_WIRE' に直しました！
        layout.operator(operators.MYADDON_OT_create_stage_bounds.bl_idname, text="ステージ範囲 (ワイヤーフレーム) を配置", icon='SHADING_WIRE')

        # スポーン地点配置ボタン（横に2つ並べる）
        row = layout.row()
        op_player = row.operator(operators.MYADDON_OT_create_spawn_point.bl_idname, text="自機スポーン", icon='OUTLINER_OB_ARMATURE')
        op_player.spawn_type = "PLAYER" # 引数として PLAYER を渡す
        
        op_enemy = row.operator(operators.MYADDON_OT_create_spawn_point.bl_idname, text="敵リスポーン地点", icon='OUTLINER_OB_ARMATURE')
        op_enemy.spawn_type = "ENEMY"   # 引数として ENEMY を渡す

        # エクスポート用の区切り線、ラベル、実行ボタン
        layout.separator()
        layout.label(text="Export")
        layout.operator(operators.MYADDON_OT_export_scene.bl_idname, text="Scene Export")


classes = (
    TOPBAR_MT_my_menu,
    OBJECT_PT_file_name,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_editor_menus.append(TOPBAR_MT_my_menu.submenu)

def unregister():
    bpy.types.TOPBAR_MT_editor_menus.remove(TOPBAR_MT_my_menu.submenu)
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
