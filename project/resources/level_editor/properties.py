import bpy

def register():
    # プロパティ登録
    bpy.types.Scene.myaddon_custom_string = bpy.props.StringProperty(name="カスタム文字列")
    bpy.types.Object.base_name = bpy.props.StringProperty(name="ベース名", default="None")

    bpy.types.Object.collider = bpy.props.StringProperty(name="タイプ", default="None")
    bpy.types.Object.collider_center = bpy.props.FloatVectorProperty(name="中心", size=3, default=(0.0, 0.0, 0.0))
    bpy.types.Object.collider_size = bpy.props.FloatVectorProperty(name="サイズ", size=3, default=(2.0, 2.0, 2.0))
    bpy.types.Object.enemy_type = bpy.props.StringProperty(name="敵タイプ", default="None")

    # 何用なのかを選ぶプルダウンメニュー！
    bpy.types.Object.game_obj_type = bpy.props.EnumProperty(
        name="オブジェクト種類",
        items=[
            ('NONE', "未設定", ""),
            ('PLAYER', "プレイヤー (Player)", ""),
            ('ENEMY', "敵 (Enemy)", ""),
            ('OBSTACLE', "障害物 (Obstacle)", "")
        ],
        default='NONE'
    )



def unregister():
    # プロパティ削除
    del bpy.types.Scene.myaddon_custom_string
    del bpy.types.Object.base_name
    del bpy.types.Object.collider
    del bpy.types.Object.collider_center
    del bpy.types.Object.collider_size
    del bpy.types.Object.enemy_type
    del bpy.types.Object.game_obj_type
