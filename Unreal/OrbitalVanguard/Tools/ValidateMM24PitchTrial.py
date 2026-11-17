"""MM24 시험 분리의 연결·경계와 5개 상하 각도 간섭 조사. 입력은 수정하지 않는다."""
import hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils.bvhtree import BVHTree
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=1:raise ValueError('새 JSON 보고서 경로가 필요합니다')
out=Path(args[0]).resolve();source=Path(bpy.data.filepath).resolve()
if out.exists():raise FileExistsError(out)
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);cradle=bpy.data.objects.get('MM24_CradleTrial');pod=bpy.data.objects.get('MM24_PitchPodTrial');base=bpy.data.objects.get('MM24_BaseTrial')
if not all((cradle,pod,base)):raise RuntimeError('시험 부품이 없습니다')
parts={}
for label,obj in [('cradle',cradle),('pod',pod),('base',base)]:
 bm=bmesh.new();bm.from_mesh(obj.data)
 parts[label]={'faces':len(bm.faces),'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'uv_layers':[u.name for u in obj.data.uv_layers]};bm.free()
def tree(obj):
 bm=bmesh.new();bm.from_mesh(obj.data)
 caps=[f for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name.startswith(('MM24_TrialPitchCut','TrialCutCap'))]
 if caps:bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
 bm.transform(obj.matrix_world);result=BVHTree.FromBMesh(bm,epsilon=0);bm.free();return result
bpy.context.view_layer.update();cradle_tree=tree(cradle);base_tree=tree(base);angles=[]
for angle in (-30,-15,0,15,30):
 pod.rotation_euler.x=math.radians(angle);bpy.context.view_layer.update();trial=tree(pod)
 angles.append({'degrees':angle,'pod_cradle_intersections':len(cradle_tree.overlap(trial)),'pod_base_intersections':len(base_tree.overlap(trial))})
pod.rotation_euler.x=0;after=sha(source)
result={'source_before':before,'source_after':after,'parent_correct':pod.parent==cradle,'parts':parts,'angles':angles,'scope':'임시 절단면 제외 표면 교차 표본. 접촉/관통 구분·전체 각도·재질·실전 미검증'}
out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if before!=after:raise RuntimeError('시험 입력이 변경됐습니다')
print('OV_MM24_PITCH_REPORT',out)
