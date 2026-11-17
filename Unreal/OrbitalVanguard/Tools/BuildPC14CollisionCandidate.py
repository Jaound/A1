"""Create a disposable PC14 combat-box candidate from UE ray samples, never overwrite inputs.

Training uses every 10 cm in Y; 5 cm rows are held out for a rough check.  This
is an approximation experiment, not a validated full triangle collision mesh.
"""
import hashlib
import json
from pathlib import Path
import sys

if len(sys.argv)!=3: raise SystemExit('Pass existing UE grid JSON and a new output JSON')
source=Path(sys.argv[1]).resolve(); output=Path(sys.argv[2]).resolve()
if output.exists(): raise FileExistsError(output)
raw=source.read_bytes(); data=json.loads(raw.decode('utf-8-sig')); rows=data['samples']; boxes=[]
for x in range(-130,131,10):
    training=[r for r in rows if abs(r['x_cm']-x)<5 and abs(r['y_cm']/10-round(r['y_cm']/10))<1e-6 and r['top_z_cm'] is not None and r['bottom_z_cm'] is not None]
    if not training: continue
    low_y=max(-75,min(r['y_cm'] for r in training)-5); high_y=min(75,max(r['y_cm'] for r in training)+5)
    low_z=max(-30,min(r['bottom_z_cm'] for r in training)-1.5); high_z=min(30,max(r['top_z_cm'] for r in training)+1.5)
    boxes.append({'local_center_cm':[x,(low_y+high_y)/2,(low_z+high_z)/2], 'half_extent_cm':[5,(high_y-low_y)/2,(high_z-low_z)/2]})

def contains(box, x,y,z):
    return all(abs(v-c)<=h+1e-5 for v,c,h in zip((x,y,z),box['local_center_cm'],box['half_extent_cm']))
held=[r for r in rows if abs(r['y_cm']/10-round(r['y_cm']/10))>1e-6]
hit=[r for r in held if r['top_z_cm'] is not None and r['bottom_z_cm'] is not None]
miss=[r for r in held if r['top_z_cm'] is None]
covered=sum(all(any(contains(b,r['x_cm'],r['y_cm'],r[k]) for b in boxes) for k in ('top_z_cm','bottom_z_cm')) for r in hit)
false_column=sum(any(abs(r['x_cm']-b['local_center_cm'][0])<=b['half_extent_cm'][0]+1e-5 and abs(r['y_cm']-b['local_center_cm'][1])<=b['half_extent_cm'][1]+1e-5 for b in boxes) for r in miss)
result={'kind':'PC14 충돌 상자 후보: UE 표면 시험 근사','source_sha256':hashlib.sha256(raw).hexdigest(),
        'method':'Y 10cm 간격 표본으로 제작하고 사용하지 않은 5cm 중간 표본으로 확인. 각 X열의 Y/Z 범위를 상자로 감싸 Y 5cm·Z 1.5cm 확장.',
        'not_final':True,'selection_obb_unchanged':True,'boxes':boxes,'heldout':{'hit_columns':len(hit),'both_endpoints_covered':covered,'miss_columns':len(miss),'miss_columns_inside_box_xy':false_column},
        'limitations':['상하의 이산 표본만 사용해 삼각형 전체와 측면·비스듬한 방향을 보장하지 않음','외형보다 넓게 덮거나 세부가 누락될 수 있음','장갑면·시설·명중·성능·실제 게임 적용은 별도 검증']}
output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('boxes',len(boxes),'heldout',result['heldout'])
