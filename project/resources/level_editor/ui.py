import bpy
import operators

_MENU_HANDLER_KEY = "cg2_level_editor_topbar_menu_handler"


def _is_my_topbar_handler(draw_func):
    qualname = getattr(draw_func, "__qualname__", "")
    return qualname.endswith("TOPBAR_MT_my_menu.submenu")


def _remove_topbar_menu_handler(draw_func):
    try:
        bpy.types.TOPBAR_MT_editor_menus.remove(draw_func)
    except Exception:
        pass


def _cleanup_old_topbar_menu_handlers():
    previous_handler = bpy.app.driver_namespace.pop(_MENU_HANDLER_KEY, None)
    if previous_handler:
        _remove_topbar_menu_handler(previous_handler)

    draw_funcs = getattr(bpy.types.TOPBAR_MT_editor_menus, "_draw_funcs", None)
    if not draw_funcs:
        return

    for draw_func in list(draw_funcs):
        if _is_my_topbar_handler(draw_func):
            _remove_topbar_menu_handler(draw_func)


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
        if obj.type == 'MESH':
            game_box.prop(obj, "game_model_file", text="外部モデル")

        # 敵が選ばれた時だけ、敵のタイプを入力させる！
        if obj.game_obj_type == 'PLAYER':
            game_box.prop(obj, "rotation_euler", index=2, text="向き")

        if obj.game_obj_type == 'ENEMY':
            game_box.prop(obj, "enemy_type", text="敵のタイプ")
            game_box.prop(obj, "enemy_path_id", text="飛行パスID")
            game_box.separator()
            game_box.label(text="Reinforcement")
            game_box.prop_search(obj, "enemy_reinforcement_trigger_name", context.scene, "objects", text="倒されたら出現")
            game_box.prop(obj, "enemy_reinforcement_delay_frames", text="ディレイ(F)")
            game_box.operator(
                operators.MYADDON_OT_assign_selected_reinforcement_trigger.bl_idname,
                text="選択敵をトリガーに設定",
                icon='LINKED',
            )

        batch_row = game_box.row()
        batch_row.operator(
            operators.MYADDON_OT_apply_active_properties_to_selection.bl_idname,
            text="選択中へ一括適用",
            icon='COPYDOWN',
        )

        if obj.type == 'CURVE' and (obj.name.startswith("EnemyPath") or getattr(obj, "enemy_path_id", "None") != "None"):
            path_box = layout.box()
            path_box.label(text="Enemy Flight Path")
            path_box.prop(obj, "enemy_path_id", text="パスID")
            path_box.prop(obj, "enemy_path_loop", text="ループ")
            path_box.prop(obj, "enemy_path_speed", text="速度")

        validation_box = layout.box()
        validation_box.label(text="Auto Validation")
        validation_box.prop(context.scene, "myaddon_auto_validation", text="自動チェック")
        validation_box.operator(operators.MYADDON_OT_validate_scene.bl_idname, text="今すぐチェック", icon='FILE_REFRESH')

        validation_status = getattr(context.scene, "myaddon_validation_status", "未チェック")
        validation_errors = getattr(context.scene, "myaddon_validation_errors", "").strip()
        validation_warnings = getattr(context.scene, "myaddon_validation_warnings", "").strip()
        validation_checks = getattr(context.scene, "myaddon_validation_checks", "").strip()
        status_icon = 'CHECKBOX_HLT'
        if validation_errors:
            status_icon = 'ERROR'
        elif validation_warnings:
            status_icon = 'INFO'

        validation_box.label(text=validation_status, icon=status_icon)

        if validation_checks:
            validation_box.separator()
            validation_box.label(text="Check Items")
            for line in validation_checks.splitlines():
                parts = line.split("\t", 2)
                if len(parts) < 3:
                    continue

                level, label, detail = parts
                icon = 'CHECKBOX_HLT'
                status_text = "OK"
                if level == "ERROR":
                    icon = 'ERROR'
                    status_text = "エラー"
                elif level == "WARNING":
                    icon = 'INFO'
                    status_text = "警告"

                validation_box.label(text=f"{status_text}: {label}", icon=icon)
                if detail:
                    validation_box.label(text=f"  {detail}")
        else:
            for message in validation_errors.splitlines():
                validation_box.label(text=message, icon='ERROR')

            for message in validation_warnings.splitlines():
                validation_box.label(text=message, icon='INFO')

        # ★修正1：区切り線のインデント（字下げ）を左に戻しました！
        layout.separator()
        layout.label(text="=== レベル構築ツール ===")
        
        # ★修正2：アイコンを 'WIRE' から 'SHADING_WIRE' に直しました！
        layout.operator(operators.MYADDON_OT_create_stage_bounds.bl_idname, text="ステージ範囲 (ワイヤーフレーム) を配置", icon='SHADING_WIRE')
        layout.prop(context.scene, "myaddon_show_spawn_forward", text="スポーン向き表示")

        # スポーン地点配置ボタン（横に2つ並べる）
        row = layout.row()
        op_player = row.operator(operators.MYADDON_OT_create_spawn_point.bl_idname, text="自機スポーン", icon='OUTLINER_OB_ARMATURE')
        op_player.spawn_type = "PLAYER" # 引数として PLAYER を渡す
        
        op_enemy = row.operator(operators.MYADDON_OT_create_spawn_point.bl_idname, text="敵リスポーン地点", icon='OUTLINER_OB_ARMATURE')
        op_enemy.spawn_type = "ENEMY"   # 引数として ENEMY を渡す

        path_row = layout.row()
        path_row.operator(operators.MYADDON_OT_create_enemy_path.bl_idname, text="敵飛行パスを配置", icon='CURVE_BEZCURVE')
        path_row.operator(operators.MYADDON_OT_assign_selected_enemy_path.bl_idname, text="選択パスを敵に割り当て", icon='LINKED')

        ai_box = layout.box()
        ai_box.label(text="AI敵プラン")
        ai_count_row = ai_box.row(align=True)
        ai_count_row.prop(context.scene, "myaddon_ai_enemy_count", text="敵数")
        ai_count_row.prop(context.scene, "myaddon_ai_enemy_seed", text="シード")
        ai_box.prop(context.scene, "myaddon_ai_enemy_provider", text="生成方式")
        ai_box.prop(context.scene, "myaddon_ai_enemy_style", text="登場スタイル")
        ai_box.prop(context.scene, "myaddon_ai_enemy_motion_prompt", text="動き")
        if getattr(context.scene, "myaddon_ai_enemy_provider", 'BUILTIN') == 'GEMINI':
            gemini_box = ai_box.box()
            gemini_box.prop(context.window_manager, "myaddon_ai_enemy_gemini_api_key", text="APIキー")
            gemini_box.prop(context.scene, "myaddon_ai_enemy_gemini_model", text="モデル")
            gemini_box.prop(context.scene, "myaddon_ai_enemy_gemini_timeout", text="待ち時間(秒)")
            gemini_box.prop(context.scene, "myaddon_ai_enemy_gemini_fallback", text="失敗時は内蔵AI")
        ai_box.prop(context.scene, "myaddon_ai_enemy_wave_delay", text="増援間隔(F)")
        ai_box.prop(context.scene, "myaddon_ai_enemy_clear_existing", text="前回生成を削除")
        ai_box.operator(operators.MYADDON_OT_ai_generate_enemy_plan.bl_idname, text="AIで敵プラン生成", icon='MOD_PARTICLES')

        # エクスポート用の区切り線、ラベル、実行ボタン
        layout.separator()
        layout.label(text="Export")
        layout.label(text="Blender保存時にscene.json / 個別OBJ / MTLが自動出力されます", icon='INFO')
        export_row = layout.row()
        export_row.operator(operators.MYADDON_OT_export_scene.bl_idname, text="Scene Export", icon='EXPORT')
        export_row.operator(operators.MYADDON_OT_playtest_game.bl_idname, text="ゲームをプレイ", icon='PLAY')
        layout.prop(context.scene, "myaddon_game_exe_path", text="ゲームEXE")


classes = (
    TOPBAR_MT_my_menu,
    OBJECT_PT_file_name,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    _cleanup_old_topbar_menu_handlers()
    bpy.types.TOPBAR_MT_editor_menus.append(TOPBAR_MT_my_menu.submenu)
    bpy.app.driver_namespace[_MENU_HANDLER_KEY] = TOPBAR_MT_my_menu.submenu

def unregister():
    _cleanup_old_topbar_menu_handlers()
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
