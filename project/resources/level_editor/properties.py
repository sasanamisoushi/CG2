import bpy

class MYADDON_PropertyGroup_AIChatMessage(bpy.types.PropertyGroup):
    role: bpy.props.StringProperty(name="Role", default="USER")
    content: bpy.props.StringProperty(name="Content", default="")
    seed: bpy.props.IntProperty(name="Seed", default=1)

def register():
    bpy.utils.register_class(MYADDON_PropertyGroup_AIChatMessage)
    bpy.types.Scene.myaddon_ai_chat_history = bpy.props.CollectionProperty(type=MYADDON_PropertyGroup_AIChatMessage)
    bpy.types.Scene.myaddon_ai_enemy_chat_history = bpy.props.CollectionProperty(type=MYADDON_PropertyGroup_AIChatMessage)
    # プロパティ登録
    bpy.types.Scene.myaddon_custom_string = bpy.props.StringProperty(name="カスタム文字列")
    bpy.types.Scene.myaddon_game_exe_path = bpy.props.StringProperty(
        name="ゲームEXE",
        description="空欄なら generated/outputs/Development/CG2.exe を起動します",
        default="",
        subtype='FILE_PATH',
    )
    bpy.types.Scene.myaddon_show_spawn_forward = bpy.props.BoolProperty(
        name="スポーン向き表示",
        default=True,
    )
    bpy.types.Scene.myaddon_ai_enemy_count = bpy.props.IntProperty(
        name="AI敵数",
        default=6,
        min=1,
        max=24,
    )
    bpy.types.Scene.myaddon_ai_enemy_seed = bpy.props.IntProperty(
        name="AIシード",
        description="同じ値なら同じ敵配置と飛行ルートを生成します",
        default=1,
        min=0,
    )
    bpy.types.Scene.myaddon_ai_enemy_prompt = bpy.props.StringProperty(
        name="敵コンセプト",
        description="例: VF1-1を3体、時計回りに円を描くように / 最初の敵が倒されたら次の敵を出す",
        default="左右に旋回して接近",
    )
    bpy.types.Scene.myaddon_ai_enemy_base_type = bpy.props.StringProperty(
        name="敵のタイプ",
        description="生成される敵に割り当てるデフォルトの種類 (例: VF1)",
        default="VF1"
    )
    bpy.types.Scene.myaddon_ai_enemy_base_path_id = bpy.props.StringProperty(
        name="飛行パスID",
        description="指定すると、新しくパスを生成せず既存のパスを指定します",
        default=""
    )
    bpy.types.Scene.myaddon_ai_enemy_trigger_target = bpy.props.PointerProperty(
        name="倒されたら出現",
        description="このターゲットが倒された時に生成した敵を出撃させます",
        type=bpy.types.Object
    )
    bpy.types.WindowManager.myaddon_ai_enemy_gemini_api_key = bpy.props.StringProperty(
        name="Gemini APIキー",
        description="このBlender起動中だけ使う一時入力です。空欄なら環境変数 GEMINI_API_KEY を使います",
        default="",
        subtype='PASSWORD',
        options={'SKIP_SAVE'},
    )
    bpy.types.Scene.myaddon_ai_enemy_gemini_model = bpy.props.EnumProperty(
        name="Geminiモデル",
        items=[
            ('gemini-3.5-flash', "Gemini 3.5 Flash", "安定版。敵ルート生成の標準候補です"),
            ('gemini-flash-latest', "Gemini Flash Latest", "Flash系の最新エイリアスを使います"),
            ('gemini-2.5-flash', "Gemini 2.5 Flash", "低遅延と品質のバランスが良いモデルです"),
            ('gemini-2.5-flash-lite', "Gemini 2.5 Flash-Lite", "軽く速いモデルです"),
            ('gemini-2.5-pro', "Gemini 2.5 Pro", "複雑なルート指定をじっくり考えさせたい時向けです"),
        ],
        default='gemini-3.5-flash',
    )
    bpy.types.Scene.myaddon_ai_enemy_gemini_timeout = bpy.props.IntProperty(
        name="待ち時間(秒)",
        default=25,
        min=5,
        max=120,
    )
    bpy.types.Scene.myaddon_ai_enemy_gemini_fallback = bpy.props.BoolProperty(
        name="失敗時は内蔵AIで生成",
        default=True,
    )
    bpy.types.Scene.myaddon_ai_enemy_wave_delay = bpy.props.IntProperty(
        name="増援間隔(F)",
        default=90,
        min=0,
    )
    bpy.types.Scene.myaddon_ai_enemy_clear_existing = bpy.props.BoolProperty(
        name="前回のAI生成を削除",
        default=True,
    )
    
    # --- AI Level Generator Properties ---
    bpy.types.Scene.myaddon_ai_level_prompt = bpy.props.StringProperty(
        name="コンセプト",
        description="例：市街地 / 迷路 / 防衛線 / ランダムに散らす / 壁を多く",
        default="市街地",
    )
    bpy.types.Scene.myaddon_ai_level_count = bpy.props.IntProperty(
        name="障害物の数",
        description="配置する障害物の基本数",
        default=16,
        min=1,
        max=500,
    )
    bpy.types.Scene.myaddon_ai_level_seed = bpy.props.IntProperty(
        name="シード",
        description="配置のランダム性を変える",
        default=1,
        min=0,
    )
    bpy.types.Scene.myaddon_ai_level_clear_existing = bpy.props.BoolProperty(
        name="前回生成の障害物を削除",
        default=True,
    )

    bpy.types.Object.base_name = bpy.props.StringProperty(name="ベース名", default="None")

    bpy.types.Object.collider = bpy.props.StringProperty(name="タイプ", default="None")
    bpy.types.Object.collider_center = bpy.props.FloatVectorProperty(name="中心", size=3, default=(0.0, 0.0, 0.0))
    bpy.types.Object.collider_size = bpy.props.FloatVectorProperty(name="サイズ", size=3, default=(2.0, 2.0, 2.0))
    bpy.types.Object.game_model_file = bpy.props.StringProperty(
        name="外部モデル",
        description="Blender上は軽い代理形状にして、ゲーム側では resources 内の指定OBJ/GLTFを読み込みます",
        default="",
    )
    bpy.types.Object.enemy_type = bpy.props.StringProperty(name="敵タイプ", default="None")
    bpy.types.Object.enemy_path_id = bpy.props.StringProperty(name="飛行パスID", default="None")
    bpy.types.Object.enemy_path_loop = bpy.props.BoolProperty(name="ループ", default=False)
    bpy.types.Object.enemy_path_speed = bpy.props.FloatProperty(name="速度", default=0.05, min=0.001)
    bpy.types.Object.enemy_reinforcement_trigger_name = bpy.props.StringProperty(
        name="増援トリガー",
        description="この敵を出現させるきっかけになる敵リスポーン地点名",
        default="",
    )
    bpy.types.Object.enemy_reinforcement_delay_frames = bpy.props.IntProperty(
        name="増援ディレイ",
        description="トリガー敵が倒されてから出現するまでのフレーム数",
        default=60,
        min=0,
    )

    # 何用なのかを選ぶプルダウンメニュー！
    bpy.types.Object.game_obj_type = bpy.props.EnumProperty(
        name="オブジェクト種類",
        items=[
            ('NONE', "未設定", ""),
            ('PLAYER', "プレイヤー (Player)", ""),
            ('ENEMY', "敵リスポーン地点 (Enemy Respawn)", ""),
            ('OBSTACLE', "障害物 (Obstacle)", "")
        ],
        default='NONE'
    )



def unregister():
    if hasattr(bpy.types.Scene, "myaddon_ai_chat_history"):
        del bpy.types.Scene.myaddon_ai_chat_history
        del bpy.types.Scene.myaddon_ai_enemy_chat_history
    try:
        bpy.utils.unregister_class(MYADDON_PropertyGroup_AIChatMessage)
    except:
        pass
    # プロパティ削除
    del bpy.types.Scene.myaddon_custom_string
    del bpy.types.Scene.myaddon_game_exe_path
    del bpy.types.Scene.myaddon_show_spawn_forward
    del bpy.types.Scene.myaddon_ai_enemy_count
    del bpy.types.Scene.myaddon_ai_enemy_seed
    del bpy.types.Scene.myaddon_ai_enemy_prompt
    del bpy.types.Scene.myaddon_ai_enemy_base_type
    del bpy.types.Scene.myaddon_ai_enemy_base_path_id
    del bpy.types.Scene.myaddon_ai_enemy_trigger_target
    del bpy.types.WindowManager.myaddon_ai_enemy_gemini_api_key
    del bpy.types.Scene.myaddon_ai_enemy_gemini_model
    del bpy.types.Scene.myaddon_ai_enemy_gemini_timeout
    del bpy.types.Scene.myaddon_ai_enemy_gemini_fallback
    del bpy.types.Scene.myaddon_ai_enemy_wave_delay
    del bpy.types.Scene.myaddon_ai_enemy_clear_existing
    
    del bpy.types.Scene.myaddon_ai_level_prompt
    del bpy.types.Scene.myaddon_ai_level_count
    del bpy.types.Scene.myaddon_ai_level_seed
    del bpy.types.Scene.myaddon_ai_level_clear_existing
    
    del bpy.types.Object.base_name
    del bpy.types.Object.collider
    del bpy.types.Object.collider_center
    del bpy.types.Object.collider_size
    del bpy.types.Object.game_model_file
    del bpy.types.Object.enemy_type
    del bpy.types.Object.enemy_path_id
    del bpy.types.Object.enemy_path_loop
    del bpy.types.Object.enemy_path_speed
    del bpy.types.Object.enemy_reinforcement_trigger_name
    del bpy.types.Object.enemy_reinforcement_delay_frames
    del bpy.types.Object.game_obj_type
