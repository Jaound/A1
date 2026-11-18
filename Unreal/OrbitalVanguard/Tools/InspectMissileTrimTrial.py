"""미사일 절단·축 후보의 이산 각도 표면 교차를 읽기 전용으로 기록한다."""
import argparse, hashlib, json, math, sys
from pathlib import Path
import bmesh, bpy
from mathutils.bvhtree import BVHTree

p=argparse.ArgumentParser();p.add_argument('--prefix',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);source,out=Path(bpy.data.filepath).resolve(),Path(a.output).resolve()
if not source.is_file() or out.exists():raise RuntimeError('입력 또는 새 출력 경로 확인 필요')
def sha():
    with source.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();parts={k:bpy.data.objects.get(a.prefix+'_'+v) for k,v in
    [('base','BaseTrial'),('body','YawBody'),('launcher','PitchTrial'),('axle','TrialPitchNeck')]}
if any(v is None for v in parts.values()):raise RuntimeError('후보 부품 누락')
parts['body'].location.z += .001;bpy.context.view_layer.update()
def tree(obj):
    bm=bmesh.new();bm.from_mesh(obj.data)
    caps=[f for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index]
          and obj.data.materials[f.material_index].name.startswith(('TrialCutCap','MissileTrialCutCap'))]
    if caps:bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
    bm.transform(obj.matrix_world);result=BVHTree.FromBMesh(bm,epsilon=0);bm.free();return result
base=tree(parts['base']);body=tree(parts['body']);rows=[]
for angle in (-90,-60,-30,-15,0,15,30,60,90):
    parts['launcher'].rotation_euler.x=math.radians(angle);bpy.context.view_layer.update()
    launcher=tree(parts['launcher']);axle=tree(parts['axle'])
    rows.append({'pitch':angle,'body_base':len(body.overlap(base)),'launcher_body':len(launcher.overlap(body)),
                 'launcher_base':len(launcher.overlap(base)),'axle_body':len(axle.overlap(body)),
                 'axle_base':len(axle.overlap(base))})
after=sha();out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps({'source':str(source),'source_before':before,'source_after':after,
    'trial_clearance':.001,'samples':rows,'scope':'이산 각도 표면 교차. 캡 및 의도된 축-발사 상자 연결 제외. 게임 미승인'},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
if before!=after:raise RuntimeError('원본 변경됨')
print('OV_MISSILE_TRIM_INSPECTION',out)
