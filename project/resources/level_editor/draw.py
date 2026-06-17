import bpy
import mathutils
import gpu
from gpu_extras.batch import batch_for_shader

# ---------------------------------------------------------
# コライダー描画クラス 
# ---------------------------------------------------------
class DrawCollider:
    handle = None

    @staticmethod
    def spawn_origin(object):
        if object.type == "MESH" and getattr(object, "game_obj_type", "NONE") == "ENEMY":
            local_center = sum((mathutils.Vector(corner) for corner in object.bound_box), mathutils.Vector()) / 8.0
            return object.matrix_world @ local_center
        return object.matrix_world.translation.copy()

    @staticmethod
    def spawn_forward(object):
        # ゲーム側の前方は +Z。Blender座標では +Y がそれに対応する。
        forward = object.matrix_world.to_quaternion() @ mathutils.Vector((0.0, 1.0, 0.0))
        if forward.length <= 0.0001:
            return mathutils.Vector((0.0, 1.0, 0.0))
        return forward.normalized()

    @staticmethod
    def append_spawn_forward_arrow(vertices, indices, object):
        origin = DrawCollider.spawn_origin(object)
        forward = DrawCollider.spawn_forward(object)
        arrow_length = 3.0
        head_length = 0.6
        head_width = 0.35

        start = origin + forward * 0.25
        end = origin + forward * arrow_length

        side = forward.cross(mathutils.Vector((0.0, 0.0, 1.0)))
        if side.length <= 0.0001:
            side = forward.cross(mathutils.Vector((1.0, 0.0, 0.0)))
        side.normalize()
        up = side.cross(forward)
        if up.length <= 0.0001:
            up = mathutils.Vector((0.0, 0.0, 1.0))
        else:
            up.normalize()

        head_base = end - forward * head_length
        start_index = len(vertices["pos"])
        vertices["pos"].extend([
            start,
            end,
            head_base + side * head_width,
            head_base - side * head_width,
            head_base + up * head_width,
            head_base - up * head_width,
        ])

        indices.extend([
            [start_index + 0, start_index + 1],
            [start_index + 1, start_index + 2],
            [start_index + 1, start_index + 3],
            [start_index + 1, start_index + 4],
            [start_index + 1, start_index + 5],
        ])

    @staticmethod
    def draw_collider():
        # --- 資料スライド1：頂点データの初期化（空にしておく） ---
        vertices = {"pos": []}
        indices = []
        forward_vertices = {"pos": []}
        forward_indices = []
        
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

            if getattr(bpy.context.scene, "myaddon_show_spawn_forward", True) and getattr(object, "game_obj_type", "NONE") in {"PLAYER", "ENEMY"}:
                DrawCollider.append_spawn_forward_arrow(forward_vertices, forward_indices, object)
        
        # 万が一、描画するBoxが一つもなかったらここで処理を終わる（エラー防止）
        if len(vertices["pos"]) == 0 and len(forward_vertices["pos"]) == 0:
            return

        # --- 最後に1回だけ描画処理を行う（for文の外に出す） ---
        shader = gpu.shader.from_builtin('UNIFORM_COLOR')
        
        gpu.state.depth_test_set('ALWAYS') 
        gpu.state.line_width_set(2.0)
        
        shader.bind()
        if vertices["pos"]:
            # verticesは既に辞書型 {"pos": [...]} になっているのでそのまま渡す
            batch = batch_for_shader(shader, 'LINES', vertices, indices=indices)
            shader.uniform_float("color", (0.0, 1.0, 1.0, 1.0)) # 水色
            batch.draw(shader)

        if forward_vertices["pos"]:
            forward_batch = batch_for_shader(shader, 'LINES', forward_vertices, indices=forward_indices)
            shader.uniform_float("color", (1.0, 0.85, 0.0, 1.0)) # 黄色
            forward_batch.draw(shader)
        
        # 設定を元に戻す
        gpu.state.depth_test_set('LESS')
        gpu.state.line_width_set(1.0)

def register():
    # ★コライダー描画関数の登録
    DrawCollider.handle = bpy.types.SpaceView3D.draw_handler_add(
        DrawCollider.draw_collider, (), 'WINDOW', 'POST_VIEW'
    )

def unregister():
    # ★コライダー描画関数の削除
    if DrawCollider.handle:
        bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, 'WINDOW')
        DrawCollider.handle = None
