import json
import re

transcript_path = r"c:\Users\k024g\.gemini\antigravity\brain\8b50d87e-621b-48c2-ac72-776a28ec59b0\.system_generated\logs\transcript_full.jsonl"
lines_dict = {}

with open(transcript_path, "r", encoding="utf-8") as f:
    for line in f:
        try:
            entry = json.loads(line)
            content = entry.get("content", "")
            if isinstance(content, str):
                lines = content.splitlines()
                for l in lines:
                    m = re.match(r'^(\d+):\s(.*)$', l)
                    if m:
                        line_num = int(m.group(1))
                        lines_dict[line_num] = m.group(2)
        except Exception as e:
            pass

print(f"Recovered {len(lines_dict)} unique lines from OLD transcript!")
with open("recovered_lines_smart_old.txt", "w", encoding="utf-8") as f:
    for num in sorted(lines_dict.keys()):
        f.write(f"{num:04d}: {lines_dict[num]}\n")
