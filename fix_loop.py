import sys

op_file = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'

with open(op_file, 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('"loop": bool(enemy.get("loop", False)),', '"loop": bool(enemy.get("loop", True)),')

with open(op_file, 'w', encoding='utf-8') as f:
    f.write(text)
