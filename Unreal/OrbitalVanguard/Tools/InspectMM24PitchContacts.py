"""MM24 시험 포드·지지대 교차면의 위치를 읽기 전용으로 조사한다."""
import hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils.bvhtree import BVHTree
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=1:raise ValueError('새 보고서 경로가 필요합니다')
out=Path(args[0]).resolve();src=Path(bpy.data.filepath).resolve()
if out.exists():raise FileExistsError(out)
def sha():
 with src.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();body=bpy.data.objects.get('MM24_CradleTrial');pod=bpy.data.objects.get('MM24_PitchPodTrial')
if not body or not pod:raise RuntimeError('시험 부품이 없습니다')
def mesh_and_tree(obj):
 bm=bmesh.new();bm.from_mesh(obj.data)
 caps=[f for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name.startswith(('MM24_TrialPitchCut','TrialCutCap'))]
 if caps:bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
 bm.transform(obj.matrix_world);bm.faces.ensure_lookup_table();bm.faces.index_update();return bm,BVHTree.FromBMesh(bm,epsilon=0)
bpy.context.view_layer.update();bm_a,tree_a=mesh_and_tree(body);results=[]
for angle in (-30,-15,0,15,30):
 pod.rotation_euler.x=math.radians(angle);bpy.context.view_layer.update();bm_b,tree_b=mesh_and_tree(pod);pairs=tree_a.overlap(tree_b)
 centers=[bm_a.faces[i].calc_center_median() for i,j in pairs];entry={'degrees':angle,'pairs':len(pairs)}
 if centers:entry.update(min=[min(p[i] for p in centers) for i in range(3)],max=[max(p[i] for p in centers) for i in range(3)])
 results.append(entry);bm_b.free()
bm_a.free();after=sha();out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps({'source_before':before,'source_after':after,'angles':results,'scope':'절단면을 제외한 지지부 교차 삼각형의 중심 범위. 실제 접촉점·관통 여부·전체 각도는 미확정'},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if before!=after:raise RuntimeError('시험 입력이 변경됐습니다')
print('OV_MM24_PITCH_CONTACTS',out)
