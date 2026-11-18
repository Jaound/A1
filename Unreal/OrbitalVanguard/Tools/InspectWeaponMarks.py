"""사용자가 칠한 도면을 원본 ZIP과 비교해 색상 영역 후보를 기록한다.

후보는 슬롯 확정이나 3D 좌표가 아니다. 사용자 도면·원본 ZIP을 수정하지 않는다.
실행: Pillow와 NumPy가 있는 Python으로 이 파일을 실행한다.
"""
import hashlib
import io
import json
from pathlib import Path
from zipfile import ZipFile
import numpy as np
from PIL import Image

root=Path.home()/'Documents/OrbitalVanguardAssets/WeaponPlacementGuide_v1'
original=root/'함선별_무장_배치도.zip'
output=root/'표시_영역_후보_v1.json'
if output.exists(): raise RuntimeError('기존 결과를 덮어쓰지 않습니다')
panels={'top':(20,170,920,670),'bottom':(930,170,1830,670),
        'left':(20,710,920,1210),'right':(930,710,1830,1210)}
rows=[]
with ZipFile(original) as archive:
    for path in sorted((root/'도면').glob('*.png')):
        members=[n for n in archive.namelist() if Path(n).name==path.name]
        if len(members)!=1: raise RuntimeError('원본 도면 확인 실패: '+path.name)
        raw=path.read_bytes(); baseline=archive.read(members[0])
        current=np.asarray(Image.open(io.BytesIO(raw)).convert('RGB'),dtype=np.int16)
        before=np.asarray(Image.open(io.BytesIO(baseline)).convert('RGB'),dtype=np.int16)
        if current.shape!=before.shape: raise RuntimeError('크기가 변경됨: '+path.name)
        row={'file':path.name,'sha256':hashlib.sha256(raw).hexdigest(),'views':{}}
        for view,(x0,y0,x1,y1) in panels.items():
            a=current[y0:y1,x0:x1];b=before[y0:y1,x0:x1]
            delta=np.max(np.abs(a-b),axis=2)>25
            red,green,blue=a[:,:,0],a[:,:,1],a[:,:,2]
            masks={'yellow':delta&(red>120)&(green>100)&(red-blue>35)&(green-blue>25),
                   'blue':delta&(blue>110)&(blue-red>20)&(blue-green>8),
                   'red':delta&(red>110)&(red-green>30)&(red-blue>12)}
            result={}
            for color,mask in masks.items():
                seen=np.zeros(mask.shape,dtype=np.bool_);regions=[]
                for y,x in zip(*np.nonzero(mask)):
                    if seen[y,x]:continue
                    seen[y,x]=True;stack=[(int(y),int(x))];points=[]
                    while stack:
                        cy,cx=stack.pop();points.append((cy,cx))
                        for dy,dx in ((-1,0),(1,0),(0,-1),(0,1)):
                            ny,nx=cy+dy,cx+dx
                            if 0<=ny<mask.shape[0] and 0<=nx<mask.shape[1] and mask[ny,nx] and not seen[ny,nx]:
                                seen[ny,nx]=True;stack.append((ny,nx))
                    if len(points)<30:continue
                    ys=[p[0] for p in points];xs=[p[1] for p in points]
                    regions.append({'pixels':len(points),'center_px':[round(sum(xs)/len(xs)+x0,1),round(sum(ys)/len(ys)+y0,1)],
                                    'bounds_px':[min(xs)+x0,min(ys)+y0,max(xs)+x0,max(ys)+y0]})
                result[color]=regions
            row['views'][view]=result
        rows.append(row)
        print(path.name, {v:{c:len(regions) for c,regions in colors.items()} for v,colors in row['views'].items()},flush=True)
output.write_text(json.dumps({'scope':'2D 색상 영역 후보. 3D 장착점·중복·유효성은 검증되지 않음','ships':rows},ensure_ascii=False,indent=2),encoding='utf8')
