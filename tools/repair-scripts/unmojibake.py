import sys
import codecs

def restore():
    path = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\level_editor\operators.py'
    with codecs.open(path, 'r', encoding='utf-8') as f:
        corrupted_text = f.read()
    
    try:
        # Encode back to cp932 to recover the original utf-8 bytes
        original_bytes = corrupted_text.encode('cp932', errors='ignore')
        restored_text = original_bytes.decode('utf-8', errors='ignore')
        with codecs.open(path, 'w', encoding='utf-8') as f:
            f.write(restored_text)
        print('SUCCESS')
    except Exception as e:
        print('ERROR:', e)

restore()
