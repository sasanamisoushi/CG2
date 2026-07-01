import json

log_file = r'C:\Users\k024g\.gemini\antigravity\brain\3c48e713-9788-4824-9c17-4b18d6b66579\.system_generated\logs\transcript_full.jsonl'

original_cpp = None
original_h = None

with open(log_file, 'r', encoding='utf-8') as f:
    for line in f:
        try:
            data = json.loads(line)
            if data.get('type') == 'VIEW_FILE' or data.get('type') == 'RUN_COMMAND':
                content = data.get('content', '')
                if 'GamePlayScene.cpp' in content or 'GamePlayScene.cpp' in content:
                    if original_cpp is None and 'Total Lines: 3763' in content:
                        original_cpp = content
        except:
            pass

print('Found' if original_cpp else 'Not Found')
