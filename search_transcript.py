import json
import os

transcript_path = r"c:\Users\k024g\.gemini\antigravity\brain\0fb02015-a0d1-4911-abf8-fde66651840d\.system_generated\logs\transcript_full.jsonl"
with open(transcript_path, "r", encoding="utf-8") as f:
    for line in f:
        try:
            entry = json.loads(line)
            content = entry.get("content", "")
            if isinstance(content, str):
                if "def _parse_ai_motion_prompt" in content and "def _parse_ai_level_prompt" in content and len(content) > 10000:
                    with open("found_big_block.txt", "w", encoding="utf-8") as out:
                        out.write(content)
                    print(f"Found big block of size {len(content)}")
                    break
        except Exception as e:
            pass
