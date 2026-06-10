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

import bpy
import sys
import os 

# =========================================================
#  何よりも先に、現在のフォルダをPythonに教える（絶対確実な方法）
# =========================================================
# まず .blend がある場所（resources）を取得
blend_dir = os.path.dirname(bpy.data.filepath)

# そこに "level_editor" というフォルダ名をくっつけて、正しいパスを作る
script_dir = os.path.join(blend_dir, "level_editor")

# それを検索リストの「一番最初(0番目)」にねじ込む！（※ insert を使います）
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

# =========================================================
#  再ロード処理（ファイルを書き換えた時に最新にするため）
# =========================================================
# 既にキャッシュされている場合は強制リロードするためにキャッシュを削除
for mod in ["properties", "validation", "operators", "ui", "draw", "network"]:
    if mod in sys.modules:
        del sys.modules[mod]

# =========================================================
#  分割したファイルの読み込み
# =========================================================
import properties
import validation
import operators
import ui
import draw
import network

def register():
    properties.register()
    validation.register()
    operators.register()
    ui.register()
    draw.register()
    network.register()
    print("レベルエディタが有効化されました。")

def unregister():
    network.unregister()
    draw.unregister()
    ui.unregister()
    operators.unregister()
    validation.unregister()
    properties.unregister()
    print("レベルエディタが無効化されました。")

if __name__ == "__main__":
    # 既に登録されている場合は、一旦解除（お掃除）してから再登録する
    try:
        unregister()
    except:
        pass
    
    register()
