import json
import math
import os
import subprocess

import bpy
import bpy_extras
from bpy.app.handlers import persistent
from mathutils import Vector, geometry

import validation


_INVALID_FILENAME_CHARS = '<>:"/\\|?*'


def _default_scene_json_path():
    blend_dir = os.path.dirname(bpy.data.filepath)
    if not blend_dir:
        return "scene.json"
    return os.path.join(blend_dir, "scene.json")


def _default_scene_obj_path():
    if not bpy.data.filepath:
        return "scene.obj"
    return os.path.splitext(bpy.data.filepath)[0] + ".obj"


def _workspace_root_from_blend():
    if bpy.data.filepath:
        return os.path.abspath(os.path.join(os.path.dirname(bpy.data.filepath), "..", ".."))
    return os.path.abspath(os.path.join(os.getcwd(), ".."))


def _default_game_exe_path():
    workspace_root = _workspace_root_from_blend()
    return os.path.join(workspace_root, "generated", "outputs", "Development", "CG2.exe")


def _default_game_working_dir():
    return os.path.join(_workspace_root_from_blend(), "project")


def _resolve_game_exe_path(scene):
    configured_path = getattr(scene, "myaddon_game_exe_path", "").strip()
    if configured_path:
        return os.path.abspath(bpy.path.abspath(configured_path))
    return _default_game_exe_path()


def _is_obj_export_target(obj):
    if obj.type != 'MESH':
        return False

    try:
        return obj.visible_get()
    except Exception:
        return not obj.hide_get()


def _is_individual_model_export_target(obj):
    if not _is_obj_export_target(obj):
        return False
    if obj.name.startswith("StageBounds"):
        return False

    category = getattr(obj, "game_obj_type", "NONE")
    return category not in {"PLAYER", "ENEMY"}


def _safe_filename_stem(name):
    stem = name.strip()
    for char in _INVALID_FILENAME_CHARS:
        stem = stem.replace(char, "_")
    stem = "".join("_" if ord(char) < 32 else char for char in stem)
    stem = stem.strip(" .")
    return stem or "Mesh"


def _build_model_filename_map(scene):
    model_filenames = {}
    used_filenames = set()

    for obj in scene.objects:
        if not _is_individual_model_export_target(obj):
            continue

        stem = _safe_filename_stem(obj.name)
        filename = f"{stem}.obj"
        index = 2
        while filename.lower() in used_filenames:
            filename = f"{stem}_{index}.obj"
            index += 1

        used_filenames.add(filename.lower())
        model_filenames[obj.name] = filename

    return model_filenames


def _get_world_transform(obj):
    trans, rot, scale = obj.matrix_world.decompose()

    # 敵モデルをリスポーン地点として使う場合は、原点ではなく見た目の中心を送る。
    if obj.type == "MESH" and getattr(obj, "game_obj_type", "NONE") == "ENEMY":
        local_center = sum((Vector(corner) for corner in obj.bound_box), Vector()) / 8.0
        trans = obj.matrix_world @ local_center

    return trans, rot, scale


def _is_enemy_path_object(obj):
    if obj.type != 'CURVE':
        return False
    path_id = getattr(obj, "enemy_path_id", "None")
    return path_id != "None" or obj.name.startswith("EnemyPath")


def _get_enemy_path_id(obj):
    path_id = getattr(obj, "enemy_path_id", "None")
    if path_id != "None":
        return path_id
    return obj.name.split(".", 1)[0]


def _get_curve_world_points(obj):
    points = []

    def append_world_point(local_point):
        world_point = obj.matrix_world @ local_point
        if points:
            previous = Vector(points[-1])
            if (world_point - previous).length <= 0.0001:
                return
        points.append([world_point.x, world_point.y, world_point.z])

    for spline in obj.data.splines:
        if spline.type == 'BEZIER':
            bezier_points = spline.bezier_points
            segment_count = len(bezier_points)
            if segment_count < 2:
                continue
            if not spline.use_cyclic_u:
                segment_count -= 1

            for index in range(segment_count):
                current = bezier_points[index]
                next_point = bezier_points[(index + 1) % len(bezier_points)]
                samples = geometry.interpolate_bezier(
                    current.co,
                    current.handle_right,
                    next_point.handle_left,
                    next_point.co,
                    max(2, obj.data.resolution_u),
                )
                for sample in samples:
                    append_world_point(sample)
        else:
            for point in spline.points:
                w = point.co.w if point.co.w != 0.0 else 1.0
                append_world_point(Vector((point.co.x / w, point.co.y / w, point.co.z / w)))

    return points


def _unique_enemy_path_id(scene):
    used_ids = {
        getattr(obj, "enemy_path_id", "None")
        for obj in scene.objects
        if getattr(obj, "enemy_path_id", "None") != "None"
    }

    if "EnemyPath" not in used_ids:
        return "EnemyPath"

    index = 1
    while True:
        candidate = f"EnemyPath{index:03d}"
        if candidate not in used_ids:
            return candidate
        index += 1


def _build_object_data(obj, model_filenames=None):
    trans, rot, scale = _get_world_transform(obj)
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

    if obj.type == 'MESH' and model_filenames and obj.name in model_filenames:
        obj_data["model"] = model_filenames[obj.name]

    if hasattr(obj, "enemy_type") and obj.enemy_type != "None":
        obj_data["enemy"] = {
            "type": obj.enemy_type,
        }

    if getattr(obj, "game_obj_type", "NONE") == 'ENEMY':
        path_id = getattr(obj, "enemy_path_id", "None")
        if path_id != "None":
            obj_data["path_id"] = path_id

    if obj.type == 'MESH':
        mesh = obj.to_mesh()
        obj_data["vertices_count"] = len(mesh.vertices)
        obj.to_mesh_clear()
    else:
        obj_data["vertices_count"] = 0

    return obj_data


def _build_path_data(obj):
    return {
        "id": _get_enemy_path_id(obj),
        "name": obj.name,
        "loop": bool(getattr(obj, "enemy_path_loop", False)),
        "speed": float(getattr(obj, "enemy_path_speed", 0.05)),
        "points": _get_curve_world_points(obj),
    }


def _build_scene_data(scene, model_filenames=None):
    if model_filenames is None:
        model_filenames = _build_model_filename_map(scene)

    errors, warnings = validation.validate_scene(scene)

    scene_data = {
        "name": "scene",
        "objects": [],
        "paths": [],
        "validation": {
            "status": _make_validation_status(errors, warnings),
            "errors": errors,
            "warnings": warnings,
            "checks": validation.build_check_report_text(errors, warnings),
        },
    }

    for obj in scene.objects:
        if obj.type in ['MESH', 'EMPTY']:
            scene_data["objects"].append(_build_object_data(obj, model_filenames))
        elif _is_enemy_path_object(obj):
            scene_data["paths"].append(_build_path_data(obj))

    return scene_data


def _make_validation_status(errors, warnings):
    if errors:
        return f"エラー {len(errors)}件 / 警告 {len(warnings)}件"
    if warnings:
        return f"警告 {len(warnings)}件"
    return "OK"


def _export_scene_to_path(scene, filepath, model_filenames=None):
    with open(filepath, "wt", encoding="utf-8") as file:
        json.dump(_build_scene_data(scene, model_filenames), file, ensure_ascii=False, indent=4)


def _export_obj_with_current_selection(filepath):
    try:
        return bpy.ops.wm.obj_export(
            filepath=filepath,
            export_selected_objects=True,
            export_materials=True,
            export_uv=True,
            export_normals=True,
            export_triangulated_mesh=True,
            apply_modifiers=True,
            path_mode='AUTO',
            forward_axis='NEGATIVE_Z',
            up_axis='Y',
        )
    except (AttributeError, TypeError):
        try:
            return bpy.ops.export_scene.obj(
                filepath=filepath,
                use_selection=True,
                use_materials=True,
                use_mesh_modifiers=True,
                use_triangles=True,
                use_uvs=True,
                use_normals=True,
                path_mode='AUTO',
                axis_forward='-Z',
                axis_up='Y',
            )
        except AttributeError:
            bpy.ops.preferences.addon_enable(module="io_scene_obj")
            return bpy.ops.export_scene.obj(
                filepath=filepath,
                use_selection=True,
                use_materials=True,
                use_mesh_modifiers=True,
                use_triangles=True,
                use_uvs=True,
                use_normals=True,
                path_mode='AUTO',
                axis_forward='-Z',
                axis_up='Y',
            )


def _export_visible_meshes_to_obj(scene, filepath):
    export_objects = [obj for obj in scene.objects if _is_obj_export_target(obj)]
    if not export_objects:
        return 0

    context = bpy.context
    view_layer = context.view_layer
    previous_active = view_layer.objects.active
    previous_selection = list(context.selected_objects)
    previous_mode = context.object.mode if context.object else None

    try:
        if context.object and context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        bpy.ops.object.select_all(action='DESELECT')
        for obj in export_objects:
            obj.select_set(True)
        view_layer.objects.active = export_objects[0]

        _export_obj_with_current_selection(filepath)
        return len(export_objects)
    finally:
        if context.object and context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        bpy.ops.object.select_all(action='DESELECT')
        for obj in previous_selection:
            if obj.name in bpy.data.objects:
                obj.select_set(True)

        if previous_active and previous_active.name in bpy.data.objects:
            view_layer.objects.active = previous_active

        if previous_mode and previous_mode != 'OBJECT' and view_layer.objects.active:
            try:
                bpy.ops.object.mode_set(mode=previous_mode)
            except Exception:
                pass


def _export_single_mesh_to_obj(obj, filepath):
    context = bpy.context
    view_layer = context.view_layer
    previous_active = view_layer.objects.active
    previous_selection = list(context.selected_objects)
    previous_mode = context.object.mode if context.object else None
    temp_obj = None
    temp_mesh = None

    try:
        if context.object and context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        depsgraph = context.evaluated_depsgraph_get()
        evaluated_obj = obj.evaluated_get(depsgraph)
        temp_mesh = bpy.data.meshes.new_from_object(evaluated_obj, depsgraph=depsgraph)
        temp_obj = bpy.data.objects.new(obj.name, temp_mesh)
        temp_obj.matrix_world.identity()
        context.collection.objects.link(temp_obj)

        bpy.ops.object.select_all(action='DESELECT')
        temp_obj.select_set(True)
        view_layer.objects.active = temp_obj

        _export_obj_with_current_selection(filepath)
    finally:
        if context.object and context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        if temp_obj and temp_obj.name in bpy.data.objects:
            bpy.data.objects.remove(temp_obj, do_unlink=True)
        if temp_mesh and temp_mesh.name in bpy.data.meshes and temp_mesh.users == 0:
            bpy.data.meshes.remove(temp_mesh)

        bpy.ops.object.select_all(action='DESELECT')
        for selected_obj in previous_selection:
            if selected_obj.name in bpy.data.objects:
                selected_obj.select_set(True)

        if previous_active and previous_active.name in bpy.data.objects:
            view_layer.objects.active = previous_active

        if previous_mode and previous_mode != 'OBJECT' and view_layer.objects.active:
            try:
                bpy.ops.object.mode_set(mode=previous_mode)
            except Exception:
                pass


def _export_individual_meshes_to_obj(scene, model_filenames):
    if not bpy.data.filepath:
        return 0

    blend_dir = os.path.dirname(bpy.data.filepath)
    exported_count = 0

    for obj in scene.objects:
        filename = model_filenames.get(obj.name)
        if not filename:
            continue

        _export_single_mesh_to_obj(obj, os.path.join(blend_dir, filename))
        exported_count += 1

    return exported_count


def _auto_export_scene_json(model_filenames=None):
    filepath = _default_scene_json_path()
    errors, warnings = validation.validate_scene(bpy.context.scene)
    if errors:
        print(f"scene.json 自動出力をスキップしました。バリデーションエラー {len(errors)}件があります。")
        return

    try:
        _export_scene_to_path(bpy.context.scene, filepath, model_filenames)
        if warnings:
            print(f"scene.json を自動出力しました（警告 {len(warnings)}件）: {filepath}")
        else:
            print(f"scene.json を自動出力しました: {filepath}")
    except Exception as exc:
        print(f"scene.json 自動出力に失敗しました: {exc}")


def _auto_export_obj(model_filenames=None):
    scene = bpy.context.scene
    if model_filenames is None:
        model_filenames = _build_model_filename_map(scene)

    try:
        exported_count = _export_individual_meshes_to_obj(scene, model_filenames)
        if exported_count == 0:
            print("OBJ/MTL 自動出力をスキップしました。表示中のメッシュがありません。")
        else:
            print(f"個別OBJ/MTLを自動出力しました（{exported_count}個のメッシュ）")
    except Exception as exc:
        print(f"OBJ/MTL 自動出力に失敗しました: {exc}")


def _unregister_auto_export_handler():
    for handler in list(bpy.app.handlers.save_post):
        if getattr(handler, "__name__", "") in {
            "_auto_export_scene_json_on_save",
            "_auto_export_assets_on_save",
        }:
            bpy.app.handlers.save_post.remove(handler)


@persistent
def _auto_export_assets_on_save(_dummy):
    if not bpy.data.filepath:
        return

    model_filenames = _build_model_filename_map(bpy.context.scene)
    _auto_export_scene_json(model_filenames)
    _auto_export_obj(model_filenames)


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
        return _build_object_data(obj)

    def _build_path_data(self, obj):
        return _build_path_data(obj)

    def export(self):
        if not self.filepath:
            self.filepath = _default_scene_json_path()

        _export_scene_to_path(bpy.context.scene, self.filepath)

    def invoke(self, context, event):
        self.filepath = _default_scene_json_path()
        return self.execute(context)

    def execute(self, context):
        if not self.filepath:
            self.filepath = _default_scene_json_path()

        errors, warnings = validation.validate_and_store(context.scene)
        if errors:
            self.report({'ERROR'}, "バリデーションエラーがあります。詳細はパネルを確認してください。")
            return {'CANCELLED'}

        self.export()
        if warnings:
            self.report({'WARNING'}, f"警告 {len(warnings)}件がありますが、シーン情報をExportしました: {self.filepath}")
            return {'FINISHED'}

        self.report({'INFO'}, f"シーン情報をExportしました: {self.filepath}")
        return {'FINISHED'}


class MYADDON_OT_validate_scene(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_validate_scene"
    bl_label = "シーンチェック"
    bl_description = "現在のシーンをゲーム用データとして使えるかチェックします"
    bl_options = {'REGISTER'}

    def execute(self, context):
        errors, warnings = validation.validate_and_store(context.scene)
        if errors:
            self.report({'ERROR'}, f"バリデーションエラー {len(errors)}件")
        elif warnings:
            self.report({'WARNING'}, f"バリデーション警告 {len(warnings)}件")
        else:
            self.report({'INFO'}, "バリデーションOK")
        return {'FINISHED'}


class MYADDON_OT_playtest_game(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_playtest_game"
    bl_label = "ゲームをプレイ"
    bl_description = "現在の配置を出力してゲームを起動します"
    bl_options = {'REGISTER'}

    def execute(self, context):
        scene = context.scene
        errors, warnings = validation.validate_and_store(scene)
        if errors:
            self.report({'ERROR'}, "バリデーションエラーがあります。修正してからPlaytestしてください。")
            return {'CANCELLED'}

        model_filenames = _build_model_filename_map(scene)
        _export_scene_to_path(scene, _default_scene_json_path(), model_filenames)
        if bpy.data.filepath:
            _export_individual_meshes_to_obj(scene, model_filenames)

        exe_path = _resolve_game_exe_path(scene)
        if not os.path.isfile(exe_path):
            self.report({'ERROR'}, f"ゲームEXEが見つかりません: {exe_path}")
            return {'CANCELLED'}

        working_dir = _default_game_working_dir()
        if not os.path.isdir(working_dir):
            working_dir = os.path.dirname(exe_path)

        try:
            subprocess.Popen([exe_path], cwd=working_dir)
        except Exception as exc:
            self.report({'ERROR'}, f"ゲーム起動に失敗しました: {exc}")
            return {'CANCELLED'}

        if warnings:
            self.report({'WARNING'}, f"警告 {len(warnings)}件がありますが、ゲームを起動しました。")
        else:
            self.report({'INFO'}, "ゲームを起動しました。")
        return {'FINISHED'}


class MYADDON_OT_apply_active_properties_to_selection(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_apply_active_properties_to_selection"
    bl_label = "選択中へ一括適用"
    bl_description = "アクティブオブジェクトのゲーム用設定を、選択中の他オブジェクトへコピーします"
    bl_options = {'REGISTER', 'UNDO'}

    copy_collider: bpy.props.BoolProperty(name="コライダーもコピー", default=True)

    def execute(self, context):
        active = context.active_object
        if not active:
            self.report({'ERROR'}, "コピー元にするオブジェクトをアクティブにしてください。")
            return {'CANCELLED'}

        targets = [obj for obj in context.selected_objects if obj != active]
        if not targets:
            self.report({'ERROR'}, "コピー先にするオブジェクトも一緒に選択してください。")
            return {'CANCELLED'}

        property_names = [
            "game_obj_type",
            "enemy_type",
            "enemy_path_id",
        ]

        if self.copy_collider:
            property_names.extend([
                "collider",
                "collider_center",
                "collider_size",
            ])

        if _is_enemy_path_object(active):
            property_names.extend([
                "enemy_path_loop",
                "enemy_path_speed",
            ])

        copied_count = 0
        for target in targets:
            for property_name in property_names:
                if not hasattr(active, property_name) or not hasattr(target, property_name):
                    continue
                value = getattr(active, property_name)
                if hasattr(value, "__len__") and not isinstance(value, str):
                    value = tuple(value)
                setattr(target, property_name, value)
            copied_count += 1

        validation.validate_and_store(context.scene)
        self.report({'INFO'}, f"{copied_count}個のオブジェクトへ設定を一括適用しました。")
        return {'FINISHED'}


class MYADDON_OT_create_enemy_path(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_create_enemy_path"
    bl_label = "敵飛行パスを配置"
    bl_description = "敵機が追従するスプラインパスを作ります"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        scene = context.scene
        active_enemy = context.active_object if context.active_object and getattr(context.active_object, "game_obj_type", "NONE") == "ENEMY" else None
        path_id = _unique_enemy_path_id(scene)

        curve = bpy.data.curves.new(path_id, 'CURVE')
        curve.dimensions = '3D'
        curve.resolution_u = 24
        curve.bevel_depth = 0.04
        curve.bevel_resolution = 2

        spline = curve.splines.new('BEZIER')
        spline.bezier_points.add(3)

        base = active_enemy.location if active_enemy else Vector((0.0, 0.0, 0.0))
        offsets = [
            Vector((0.0, 0.0, 0.0)),
            Vector((0.0, 5.0, 2.0)),
            Vector((3.0, 10.0, 4.0)),
            Vector((0.0, 15.0, 6.0)),
        ]

        for point, offset in zip(spline.bezier_points, offsets):
            point.co = base + offset
            point.handle_left_type = 'AUTO'
            point.handle_right_type = 'AUTO'

        obj = bpy.data.objects.new(path_id, curve)
        context.collection.objects.link(obj)
        obj.enemy_path_id = path_id
        obj.enemy_path_loop = False
        obj.enemy_path_speed = 0.05

        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        context.view_layer.objects.active = obj

        if active_enemy:
            active_enemy.enemy_path_id = path_id

        print(f"敵飛行パスを配置しました: {path_id}")
        return {'FINISHED'}


class MYADDON_OT_assign_selected_enemy_path(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_assign_selected_enemy_path"
    bl_label = "選択パスを敵に割り当て"
    bl_description = "選択中の敵リスポーン地点へ、同時選択しているパスを割り当てます"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        active = context.active_object
        if not active or getattr(active, "game_obj_type", "NONE") != "ENEMY":
            self.report({'ERROR'}, "割り当て先の敵リスポーン地点をアクティブにしてください。")
            return {'CANCELLED'}

        selected_paths = [
            obj for obj in context.selected_objects
            if obj != active and _is_enemy_path_object(obj)
        ]

        if not selected_paths:
            self.report({'ERROR'}, "割り当てる EnemyPath カーブも一緒に選択してください。")
            return {'CANCELLED'}

        active.enemy_path_id = _get_enemy_path_id(selected_paths[0])
        self.report({'INFO'}, f"{active.name} に {active.enemy_path_id} を割り当てました。")
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
            obj.name = "EnemyRespawn"
            obj.game_obj_type = 'ENEMY'
            print("敵のリスポーン地点を配置しました。")

        return {'FINISHED'}


classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
    MYADDON_OT_validate_scene,
    MYADDON_OT_playtest_game,
    MYADDON_OT_apply_active_properties_to_selection,
    MYADDON_OT_create_enemy_path,
    MYADDON_OT_assign_selected_enemy_path,
    MYADDON_OT_create_stage_bounds,
    MYADDON_OT_create_spawn_point,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    _unregister_auto_export_handler()
    bpy.app.handlers.save_post.append(_auto_export_assets_on_save)


def unregister():
    _unregister_auto_export_handler()
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
