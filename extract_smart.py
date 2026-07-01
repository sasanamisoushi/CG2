import json
import re

transcript_path = r"c:\Users\k024g\.gemini\antigravity\brain\0fb02015-a0d1-4911-abf8-fde66651840d\.system_generated\logs\transcript_full.jsonl"
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
                        # Only keep the latest viewed version of a line, or the longest one?
                        # Let's keep all, actually latest is better, so just overwrite
                        lines_dict[line_num] = m.group(2)
        except Exception as e:
            pass

print(f"Recovered {len(lines_dict)} unique lines!")
with open("recovered_lines_smart.txt", "w", encoding="utf-8") as f:
    for num in sorted(lines_dict.keys()):
        f.write(f"{num:04d}: {lines_dict[num]}\n")

# Check missing
missing_count = 0
for i in range(270, 1754):
    if i not in lines_dict:
        missing_count += 1
print(f"Missing {missing_count} lines between 270 and 1754")
