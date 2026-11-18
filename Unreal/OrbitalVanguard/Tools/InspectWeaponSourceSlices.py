"""원본 무장 9종의 수평 단면을 조사한다. 분리면을 확정하거나 메시를 변경하지 않는다."""
import hashlib
import json
import math
from pathlib import Path
import bpy
from mathutils import Vector

root=Path.home()/'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponFullSources_v1'
project=Path(__file__).resolve().parents[1]
out=project/'Saved/Validation/WeaponSourceSlices.json'
if out.exists():raise FileExistsError(out)
models=json.loads((root/'report.json').read_text(encoding='utf8'))['models']
rows=[]
for model in models:
    source=Path(model['prepared_fbx'])
    with source.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
    if digest!=model['prepared_sha256']:raise RuntimeError('SOURCE HASH '+model['id'])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source),use_image_search=False)
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
    corners=[o.matrix_world@Vector(c) for o in objects for c in o.bound_box]
    lo=min(p.z for p in corners);hi=max(p.z for p in corners)
    bins=[{'count':0,'min_xy':[math.inf,math.inf],'max_xy':[-math.inf,-math.inf]} for _ in range(100)]
    for obj in objects:
        matrix=obj.matrix_world
        for v in obj.data.vertices:
            p=matrix@v.co;index=min(99,max(0,int((p.z-lo)/max(hi-lo,1e-9)*100)));b=bins[index];b['count']+=1
            for i in range(2):b['min_xy'][i]=min(b['min_xy'][i],p[i]);b['max_xy'][i]=max(b['max_xy'][i],p[i])
    for i,b in enumerate(bins):
        b['z_min']=lo+(hi-lo)*i/100;b['z_max']=lo+(hi-lo)*(i+1)/100
        if not b['count']:b['min_xy']=b['max_xy']=None
    rows.append({'id':model['id'],'source_sha256':digest,'z_min':lo,'z_max':hi,'slices':bins})
    print('OV_WEAPON_SLICES',model['id'],len(objects),flush=True)
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps({'scope':'정적 원본 단면 측정. 피벗·관절·절단 경계나 전장 적용을 확정하지 않음.','models':rows},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
