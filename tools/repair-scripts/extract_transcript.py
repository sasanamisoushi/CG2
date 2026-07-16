import json
import re

transcript_path = r"c:\Users\k024g\.gemini\antigravity\brain\0fb02015-a0d1-4911-abf8-fde66651840d\.system_generated\logs\transcript_full.jsonl"
lines_dict = {}

with open(transcript_path, "r", encoding="utf-8") as f:
    for line in f:
        try:
            entry = json.loads(line)
            if entry.get("type") == "TOOL_RESPONSE":
                content = entry.get("content", "")
                if "operators.py" in content and "The following code has been modified to include a line number before every line" in content:
                    lines = content.splitlines()
                    for l in lines:
                        m = re.match(r'^(\d+):\s(.*)$', l)
                        if m:
                            line_num = int(m.group(1))
                            lines_dict[line_num] = m.group(2)
        except:
            pass

print(f"Recovered {len(lines_dict)} unique lines from transcript view_file outputs!")
missing_start = 270
missing_end = 1754
missing_count = 0
for i in range(missing_start, missing_end + 1):
    if i not in lines_dict:
        missing_count += 1
print(f"Missing {missing_count} lines between {missing_start} and {missing_end}")

# Write what we have
with open("recovered_lines.txt", "w", encoding="utf-8") as f:
    for num in sorted(lines_dict.keys()):
        f.write(f"{num:04d}: {lines_dict[num]}\n")
