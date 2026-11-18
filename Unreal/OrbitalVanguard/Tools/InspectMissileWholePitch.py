"""원본 상부 전체를 기울이는 대안의 표면 교차를 측정한다. 게임에 적용하지 않는다."""
import argparse, hashlib, json, math, sys
from pathlib import Path
import bmesh, bpy
from mathutils.bvhtree import BVHTree

p=argparse.ArgumentParser(); p.add_argument('--prefix',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
source,out=Path(bpy.data.filepath).resolve(),Path(a.output).resolve()
if not source.is_file() or out.exists():raise RuntimeError('입력 또는 새 출력 경로 확인 필요')
def sha():
    with source.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(); base=bpy.data.objects.get(a.prefix+'_BaseTrial'); body=bpy.data.objects.get(a.prefix+'_YawTrial')
if not base or not body:raise RuntimeError('원본 회전 후보 부품이 없습니다')
out.mkdir(parents=True)
body.location.z += .001
def tree(obj):
    bm=bmesh.new();bm.from_mesh(obj.data)
    caps=[f for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index]
          and obj.data.materials[f.material_index].name.startswith('TrialCutCap')]
    if caps:bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
    bm.transform(obj.matrix_world); result=BVHTree.FromBMesh(bm,epsilon=0);bm.free();return result
fixed=tree(base);rows=[]
for angle in (-90,-60,-30,-15,0,15,30,60,90):
    body.rotation_euler.x=math.radians(angle);bpy.context.view_layer.update()
    overlaps=len(fixed.overlap(tree(body)))
    rows.append({'pitch':angle,'surface_overlap_pairs':overlaps})
    bpy.context.scene.render.filepath=str(out/('pitch_%+04d.png'%angle));bpy.ops.render.render(write_still=True)
after=sha();(out/'report.json').write_text(json.dumps({'source':str(source),'source_before':before,'source_after':after,
    'trial_clearance':.001,'samples':rows,'scope':'받침/상부 이산 각도 표면 교차. 발사구·연속 각도·게임 미검증'},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
if before!=after:raise RuntimeError('원본이 변경되었습니다')
print('OV_MISSILE_WHOLE_PITCH',out)
