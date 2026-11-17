"""교차 표본을 이용한 MM24 지지부 불린 여유 공간 시험. 원본은 수정하지 않는다."""
import hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils import Vector
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('교차 보고서와 새 출력 폴더가 필요합니다')
evidence=Path(args[0]).resolve();out=Path(args[1]).resolve();source=Path(bpy.data.filepath).resolve()
if out.exists():raise FileExistsError(out)
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);report=json.loads(evidence.read_text(encoding='utf-8'))
if report['source_before']!=before or report['source_after']!=before:raise RuntimeError('입력 해시 불일치')
body=bpy.data.objects.get('MM24_CradleTrial');pod=bpy.data.objects.get('MM24_PitchPodTrial')
if not body or not pod or pod.parent!=body:raise RuntimeError('시험 부품이 없습니다')
# Deliberately provisional boxes derived from nonzero-angle intersection centers.
# Margins make no claim of continuous clearance or good art.
regions=[]
for group in ((-30,-15),(15,30)):
 rows=[r for r in report['angles'] if r['degrees'] in group and r.get('pairs')]
 if not rows:raise RuntimeError('교차 표본 누락')
 lo=Vector([min(r['min'][i] for r in rows)-.025 for i in range(3)])
 hi=Vector([max(r['max'][i] for r in rows)+.025 for i in range(3)])
 regions.append({'angles':list(group),'min':list(lo),'max':list(hi)})
 bpy.ops.mesh.primitive_cube_add(size=1,location=(lo+hi)*.5);cutter=bpy.context.object;cutter.name='MM24_TrialClearanceCutter'
 cutter.dimensions=hi-lo;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 bpy.ops.object.select_all(action='DESELECT');body.select_set(True);bpy.context.view_layer.objects.active=body
 modifier=body.modifiers.new('MM24_TrialClearance','BOOLEAN');modifier.operation='DIFFERENCE';modifier.solver='EXACT';modifier.object=cutter
 bpy.ops.object.modifier_apply(modifier=modifier.name);bpy.data.objects.remove(cutter,do_unlink=True)
bm=bmesh.new();bm.from_mesh(body.data);topology={'faces':len(bm.faces),'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges)};bm.free()
out.mkdir(parents=True);scene=bpy.context.scene
for angle in (0,15,-15,30,-30):
 pod.rotation_euler.x=math.radians(angle);bpy.context.view_layer.update();scene.render.filepath=str(out/('pitch_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
pod.rotation_euler.x=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/'MM24_ClearanceTrial.blend'))
after=sha(source);result={'source_before':before,'source_after':after,'evidence_sha256':sha(evidence),'regions':regions,'topology':topology,'scope':'비영구 불린 절단 시험. 외형·관절·연속 각도·재질·전장 적용 미승인'}
(out/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if before!=after:raise RuntimeError('입력이 변경됐습니다')
print('OV_MM24_CLEARANCE_TRIAL',out)
