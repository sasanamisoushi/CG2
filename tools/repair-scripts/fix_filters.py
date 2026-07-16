import io
import sys

def main():
    path = r'c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\CG2.vcxproj.filters'
    with io.open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    text = text.replace('繧ｽ繝ｼ繧ｹ 繝輔ぃ繧､繝ｫ', 'ソース ファイル')
    text = text.replace('繝倥ャ繝€繝ｼ 繝輔ぃ繧､繝ｫ', 'ヘッダー ファイル')
    text = text.replace('繝ｪ繧ｽ繝ｼ繧ｹ 繝輔ぃ繧､繝ｫ', 'リソース ファイル')

    with io.open(path, 'w', encoding='utf-8') as f:
        f.write(text)

    print('Fixed .vcxproj.filters')

if __name__ == '__main__':
    main()
