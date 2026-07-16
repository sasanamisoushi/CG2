with open("project/resources/level_editor/operators.py", "r", encoding="utf-8") as f:
    bad_text = f.read()

try:
    good_bytes = bad_text.encode("cp932", errors="ignore")
    good_text = good_bytes.decode("utf-8", errors="ignore")
    print(good_text[1600:2000])
except Exception as e:
    print("Error:", e)
