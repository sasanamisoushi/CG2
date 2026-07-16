import json
with open('c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/project/resources/scene.json', 'r', encoding='utf-8') as f:
    data = json.load(f)
for p in data['objects']:
    if p.get('id') == 'EnemyPath001':
        pts = p.get('points', [])
        print(f'Count: {len(pts)}')
        if len(pts) > 0:
            print(f'First: {pts[0]}')
            print(f'Mid: {pts[len(pts)//2]}')
            print(f'Last: {pts[-1]}')
