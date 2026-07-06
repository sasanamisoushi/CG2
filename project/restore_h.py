import json
import sys

def find_first_view_file(filename):
    with open(r'C:\Users\k024g\.gemini\antigravity\brain\3c48e713-9788-4824-9c17-4b18d6b66579\.system_generated\logs\transcript_full.jsonl', 'r', encoding='utf-8') as f:
        for line in f:
            try:
                data = json.loads(line)
                if data.get('type') == 'VIEW_FILE':
                    content = data.get('content', '')
                    if filename in content and 'The above content shows the entire, complete file contents' in content:
                        # Extract just the lines of code
                        lines = content.split('\n')
                        code_lines = []
                        for l in lines:
                            if ': ' in l and l.split(':', 1)[0].isdigit():
                                code_lines.append(l.split(': ', 1)[1].rstrip('\r'))
                        
                        return '\n'.join(code_lines)
            except:
                pass
    return None

h_code = find_first_view_file('GamePlayScene.h')
if h_code:
    with open(r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\Game\Scene\GamePlayScene.h', 'w', encoding='utf-8') as f:
        f.write(h_code)
    print("Successfully restored GamePlayScene.h")
else:
    print("Could not find full GamePlayScene.h in logs")
