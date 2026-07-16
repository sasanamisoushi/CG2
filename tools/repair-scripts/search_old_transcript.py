import json

transcript_path = r"c:\Users\k024g\.gemini\antigravity\brain\8b50d87e-621b-48c2-ac72-776a28ec59b0\.system_generated\logs\transcript_full.jsonl"
with open(transcript_path, "r", encoding="utf-8") as f:
    for line in f:
        try:
            entry = json.loads(line)
            content = entry.get("content", "")
            if isinstance(content, str):
                if "def _parse_ai_motion_prompt" in content and "def _parse_ai_level_prompt" in content and len(content) > 10000:
                    with open("old_operators.py", "w", encoding="utf-8") as out:
                        out.write(content)
                    print(f"Found old file! Size: {len(content)}")
                    break
        except Exception as e:
            pass
