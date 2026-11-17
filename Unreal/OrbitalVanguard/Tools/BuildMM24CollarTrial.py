"""MM24 시험 여유 공간에 원본 지지대에서 추출한 회전 칼라를 연결한다."""
import hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('원 지지대 시험 파일과 새 출력 폴더가 필요합니다')
original=Path(args[0]).resolve();out=Path(args[1]).resolve();source=Path(bpy.data.filepath).resolve()
if out.exists() or not original.is_file():raise RuntimeError('입력이 없거나 출력이 이미 존재합니다')
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);original_before=sha(original);body=bpy.data.objects.get('MM24_CradleTrial');pod=bpy.data.objects.get('MM24_PitchPodTrial')
if not body or not pod or pod.parent!=body:raise RuntimeError('시험 부품이 없습니다')
with bpy.data.libraries.load(str(original),link=False) as (src,dst):
 if 'MM24_CradleTrial' not in src.objects:raise RuntimeError('추출할 원 지지대가 없습니다')
 dst.objects=['MM24_CradleTrial']
collar=dst.objects[0];bpy.context.scene.collection.objects.link(collar);collar.name='MM24_SourceCollarTrial'
def cutter(radius,name):
 bpy.ops.mesh.primitive_cylinder_add(vertices=96,radius=radius,depth=1.56,location=(0,0,-.23),rotation=(0,math.pi/2,0))
 obj=bpy.context.object;obj.name=name;return obj
def apply(obj,tool,operation):
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 modifier=obj.modifiers.new('MM24_TrialCollar','BOOLEAN');modifier.operation=operation;modifier.solver='EXACT';modifier.object=tool
 bpy.ops.object.modifier_apply(modifier=modifier.name);bpy.data.objects.remove(tool,do_unlink=True)
apply(collar,cutter(.16,'MM24_ExtractCutter'),'INTERSECT')
if not collar.data.polygons:raise RuntimeError('원 지지대 칼라가 비어 있습니다')
apply(body,cutter(.22,'MM24_ClearanceCutter'),'DIFFERENCE')
bpy.ops.object.select_all(action='DESELECT');pod.select_set(True);collar.select_set(True);bpy.context.view_layer.objects.active=pod;bpy.ops.object.join()
def topology(obj):
 bm=bmesh.new();bm.from_mesh(obj.data);result={'faces':len(bm.faces),'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges)};bm.free();return result
shapes={'cradle':topology(body),'pod_and_collar':topology(pod)};out.mkdir(parents=True);scene=bpy.context.scene
for angle in (0,15,-15,30,-30):
 pod.rotation_euler.x=math.radians(angle);bpy.context.view_layer.update();scene.render.filepath=str(out/('pitch_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
pod.rotation_euler.x=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/'MM24_CollarTrial.blend'))
after=sha(source);original_after=sha(original);result={'source_before':before,'source_after':after,'original_before':original_before,'original_after':original_after,'collar_radius':.16,'clearance_radius':.22,'cylinder_depth':1.56,'topology':shapes,'scope':'원본 복사 지지대의 칼라를 사용한 시험. 외형·피벗·간섭·재질·전장 적용 미승인'}
(out/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if before!=after or original_before!=original_after:raise RuntimeError('입력 변경')
print('OV_MM24_COLLAR_TRIAL',out)
