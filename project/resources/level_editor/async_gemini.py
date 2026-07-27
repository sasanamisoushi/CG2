import bpy
import threading
import json
import urllib.request
import urllib.parse
from mathutils import Vector
import random
import operators

class OllamaWorker(threading.Thread):
    def __init__(self, model_name, prompt_text, timeout):
        super().__init__()
        self.url = "http://localhost:11434/api/generate"
        self.model_name = model_name
        self.prompt_text = prompt_text
        self.timeout = timeout
        self.result_data = None
        self.error_message = None

    def run(self):
        try:
            payload = {
                "model": self.model_name,
                "prompt": self.prompt_text,
                "format": "json",
                "stream": False
            }
            req = urllib.request.Request(
                self.url,
                data=json.dumps(payload).encode("utf-8"),
                headers={"Content-Type": "application/json"},
                method="POST"
            )
            with urllib.request.urlopen(req, timeout=self.timeout) as response:
                text = response.read().decode("utf-8")
            data = json.loads(text)
            response_text = data.get("response", "")
            
            # response_text should be JSON due to format: json
            try:
                self.result_data = json.loads(response_text)
            except Exception as e:
                # Fallback parser if Ollama returns malformed json
                self.result_data = operators._parse_gemini_json_text(response_text)
                
        except urllib.error.URLError as e:
            self.error_message = f"Ollamaに接続できません。Ollamaが起動しているか確認してください。({e.reason})"
        except Exception as e:
            self.error_message = str(e)


class MYADDON_OT_ai_generate_enemy_plan_async(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_generate_enemy_plan_async"
    bl_label = "AI敵プラン生成(非同期)"
    bl_description = "AIが敵の配置プランを考案し、非同期で配置します"
    bl_options = {'REGISTER', 'UNDO'}

    _timer = None
    _worker = None
    
    def modal(self, context, event):
        if event.type == 'TIMER':
            if self._worker and not self._worker.is_alive():
                context.window_manager.event_timer_remove(self._timer)
                if self._worker.error_message:
                    self.report({'ERROR'}, f"Ollama Error: {self._worker.error_message}")
                else:
                    self.report({'INFO'}, "AIが戦略を考案しました。配置を行います...")
                    self.apply_result(context, self._worker.result_data)
                return {'FINISHED'}
            return {'PASS_THROUGH'}
        return {'PASS_THROUGH'}
        
    def execute(self, context):
        scene = context.scene
        provider = getattr(scene, "myaddon_ai_enemy_provider", 'BUILTIN')
        
        history = list(getattr(scene, "myaddon_ai_enemy_chat_history", []))
        prompt = getattr(scene, "myaddon_ai_enemy_prompt", "").strip()
        seed = max(0, int(getattr(scene, "myaddon_ai_enemy_seed", 1)))
        
        if provider != 'OLLAMA':
            fallback_enabled = getattr(scene, "myaddon_ai_enemy_ollama_fallback", True)
            if fallback_enabled:
                from .operators import _parse_ai_enemy_prompt
                full_text = " ".join([m.content for m in history]) + " " + prompt
                _, motion = _parse_ai_enemy_prompt(full_text)
                if not motion.get("matched_keywords", False) and len(full_text.strip()) > 0:
                    provider = 'OLLAMA'
                else:
                    return bpy.ops.myaddon.myaddon_ot_ai_generate_enemy_plan()
            else:
                return bpy.ops.myaddon.myaddon_ot_ai_generate_enemy_plan()
            
        ollama_model = getattr(scene, "myaddon_ai_ollama_model", "llama3")
        
        prompt_text = (
            "You are a game AI level designer assistant.\n"
            "Analyze the conversation and determine the best parameters for enemy generation.\n"
            "Return ONLY a valid JSON object matching this exact format, with no markdown or other text: {\"style\": \"AMBUSH\"|\"SWARM\"|\"PATROL\"|\"BALANCED\", \"motion\": {\"speed_multiplier\": 1.0, \"opener\": \"ALL\"|\"ONE\"|\"NONE\", \"turn_sign\": 1.0}}\n"
            "Conversation:\n"
        )
        for m in history:
            prompt_text += f"[{m.role}] {m.content}\n"
        if prompt:
            prompt_text += f"[USER] {prompt}\n"
            
        timeout = max(5, int(getattr(scene, "myaddon_ai_enemy_ollama_timeout", 25)))
        self._worker = OllamaWorker(ollama_model, prompt_text, timeout)
        self._worker.start()
        
        wm = context.window_manager
        self._timer = wm.event_timer_add(0.1, window=context.window)
        wm.modal_handler_add(self)
        self.report({'INFO'}, "ローカルAIに指示を送信しました。バックグラウンドで思考中...")
        return {'RUNNING_MODAL'}
        
    def apply_result(self, context, data):
        scene = context.scene
        if not data:
            self.report({'WARNING'}, "データが返されませんでした")
            return
            
        forced_params = {"style": data.get("style", "BALANCED"), "motion": data.get("motion", {})}
        scene.myaddon_ai_enemy_force_params = json.dumps(forced_params)
        scene.myaddon_ai_enemy_provider = 'BUILTIN'
        bpy.ops.myaddon.myaddon_ot_ai_generate_enemy_plan()
        scene.myaddon_ai_enemy_provider = 'OLLAMA'


class MYADDON_OT_ai_generate_level_obstacles_async(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_ai_generate_level_obstacles_async"
    bl_label = "AI障害物自動配置(非同期)"
    bl_description = "AIが障害物配置プランを考案し、非同期で配置します"
    bl_options = {'REGISTER', 'UNDO'}

    _timer = None
    _worker = None
    
    def modal(self, context, event):
        if event.type == 'TIMER':
            if self._worker and not self._worker.is_alive():
                context.window_manager.event_timer_remove(self._timer)
                if self._worker.error_message:
                    self.report({'ERROR'}, f"Ollama Error: {self._worker.error_message}")
                else:
                    self.report({'INFO'}, "AIが戦略を考案しました。障害物を配置します...")
                    self.apply_result(context, self._worker.result_data)
                return {'FINISHED'}
            return {'PASS_THROUGH'}
        return {'PASS_THROUGH'}
        
    def execute(self, context):
        scene = context.scene
        ollama_model = getattr(scene, "myaddon_ai_ollama_model", "llama3")
            
        history = list(scene.myaddon_ai_chat_history)
        prompt = getattr(scene, "myaddon_ai_level_prompt", "")
        
        prompt_text = (
            "You are a game AI level designer assistant.\n"
            "Analyze the conversation and determine the best parameters for obstacle generation.\n"
            "Return ONLY a valid JSON object matching this exact format, with no markdown or other text: {\"style\": \"CITY\"|\"DEFENSE_LINE\"|\"MAZE\"|\"ARENA\"|\"RANDOM\", \"shape\": \"CUBE\"|\"CYLINDER\"|\"WALL\"|\"PILLAR\"|\"BLOCK\", \"density_multiplier\": 1.0, \"randomize_location\": true, \"match_player_size\": false}\n"
            "Conversation:\n"
        )
        for m in history:
            prompt_text += f"[{m.role}] {m.content}\n"
        if prompt:
            prompt_text += f"[USER] {prompt}\n"
            
        timeout = max(5, int(getattr(scene, "myaddon_ai_enemy_ollama_timeout", 25)))
        self._worker = OllamaWorker(ollama_model, prompt_text, timeout)
        self._worker.start()
        
        wm = context.window_manager
        self._timer = wm.event_timer_add(0.1, window=context.window)
        wm.modal_handler_add(self)
        self.report({'INFO'}, "ローカルAIに指示を送信しました。バックグラウンドで思考中...")
        return {'RUNNING_MODAL'}
        
    def apply_result(self, context, data):
        scene = context.scene
        if not data:
            self.report({'WARNING'}, "データが返されませんでした")
            return
            
        scene.myaddon_ai_level_force_params = json.dumps(data)
        bpy.ops.myaddon.myaddon_ot_ai_generate_level_obstacles()


classes = (
    MYADDON_OT_ai_generate_enemy_plan_async,
    MYADDON_OT_ai_generate_level_obstacles_async,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
