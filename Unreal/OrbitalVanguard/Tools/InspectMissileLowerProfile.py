"""발사 상자 바깥쪽의 높이 분포를 읽기 전용으로 조사한다."""
import hashlib, json, sys
from pathlib import Path
import bpy

args = sys.argv[sys.argv.index('--') + 1:]
if len(args) != 3: raise ValueError('메시 ID, 바깥쪽 X 한계, 새 보고서 경로가 필요합니다')
name, limit, target = args[0], float(args[1]), Path(args[2]).resolve()
source = Path(bpy.data.filepath).resolve(); obj = bpy.data.objects.get(name)
if not obj or obj.type != 'MESH' or target.exists() or not 0 < limit < 2: raise ValueError('입력 확인 필요')
def sha():
    with source.open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()
before = sha()
points = [obj.matrix_world @ v.co for v in obj.data.vertices if abs((obj.matrix_world @ v.co).x) >= limit]
if not points: raise RuntimeError('표본이 없습니다')
low, high = min(p.y for p in points), max(p.y for p in points)
bins = [[] for _ in range(32)]
for pt in points:
    bins[min(31, int((pt.y-low)/(high-low)*32))].append(pt.z)
data=[]
for i, row in enumerate(bins):
    row.sort()
    data.append({'y_min': low+(high-low)*i/32, 'y_max': low+(high-low)*(i+1)/32, 'count': len(row),
                 'z_min': row[0] if row else None, 'z_p05': row[int((len(row)-1)*.05)] if row else None})
after=sha(); target.parent.mkdir(parents=True,exist_ok=True)
target.write_text(json.dumps({'source':str(source),'source_before':before,'source_after':after,'mesh':name,
    'abs_x_min':limit,'bins':data,'scope':'바깥쪽 메시 정점 표본. 접합면·회전축 확정 아님'},ensure_ascii=False,indent=2),encoding='utf8')
if before != after: raise RuntimeError('원본 변경됨')
print('OV_MISSILE_LOWER_PROFILE',target)
