"""MM24 융합 회전부를 표본 근거로 복사 분리한 실험. 원본을 저장하지 않는다."""
import hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils import Matrix,Vector
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('단면 JSON과 새 출력 폴더가 필요합니다')
evidence=Path(args[0]).resolve();out=Path(args[1]).resolve();source=Path(bpy.data.filepath).resolve()
if out.exists():raise FileExistsError(out)
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);scan=json.loads(evidence.read_text(encoding='utf-8'))
if scan['source_before']!=before or scan['source_after']!=before or scan['mesh']!='MM24_YawTrial' or scan['axis']!='z':raise RuntimeError('입력과 단면 기록이 일치하지 않습니다')
original=bpy.data.objects.get('MM24_YawTrial');base=bpy.data.objects.get('MM24_BaseTrial')
if not original or not base:raise RuntimeError('시험 회전부나 받침대가 없습니다')
# The section expands near z=-0.23; this is a disposable hypothesis, not an authored joint.
cut_z=-.23;pivot=Vector((0,0,cut_z));yaw_pivot=original.matrix_world.translation.copy();scene=bpy.context.scene
out.mkdir(parents=True);cap=bpy.data.materials.new('MM24_TrialPitchCut');cap.diffuse_color=(.22,.23,.25,1);parts={};result={}
for suffix,upper in [('CradleTrial',False),('PitchPodTrial',True)]:
 mesh=original.data.copy();mesh.name='MM24_'+suffix+'Mesh';obj=bpy.data.objects.new('MM24_'+suffix,mesh);scene.collection.objects.link(obj);mesh.materials.append(cap)
 bm=bmesh.new();bm.from_mesh(mesh);bm.transform(original.matrix_world)
 cut=bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=1e-5,plane_co=pivot,plane_no=Vector((0,0,1)),clear_inner=upper,clear_outer=not upper)
 edges=[e for e in cut['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_valid and e.is_boundary]
 filled=bmesh.ops.holes_fill(bm,edges=edges,sides=0) if edges else {'faces':[]}
 for face in filled['faces']:face.material_index=len(mesh.materials)-1
 result[suffix]={'faces':len(bm.faces),'cut_edges':len(edges),'cap_faces':len(filled['faces']),'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges)}
 bm.to_mesh(mesh);bm.free();origin=pivot if upper else yaw_pivot;mesh.transform(Matrix.Translation(-origin));obj.location=origin;parts[suffix]=obj
bpy.data.objects.remove(original,do_unlink=True);pod=parts['PitchPodTrial'];cradle=parts['CradleTrial'];bpy.context.view_layer.update();world=pod.matrix_world.copy();pod.parent=cradle;pod.matrix_world=world
cam=bpy.data.cameras.new('PitchTrialCamera');camera=bpy.data.objects.new('PitchTrialCamera',cam);scene.collection.objects.link(camera);scene.camera=camera;cam.type='ORTHO';cam.ortho_scale=5.8
camera.location=Vector((7,0,1.1));camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL';scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.background_type='WORLD'
if not scene.world:scene.world=bpy.data.worlds.new('TrialWorld')
scene.world.color=(.025,.035,.05);scene.render.resolution_x=800;scene.render.resolution_y=800;scene.render.resolution_percentage=100
for angle in (0,15,-15):
 pod.rotation_euler.x=math.radians(angle);bpy.context.view_layer.update();scene.render.filepath=str(out/('pitch_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
pod.rotation_euler.x=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/'MM24_PitchTrial.blend'))
after=sha(source);report={'source':str(source),'source_before':before,'source_after':after,'evidence_sha256':sha(evidence),'cut_z':cut_z,'pitch_pivot':list(pivot),'parts':result,'status':'시험 분리만 수행. 관절·절단면·피벗·교차·재질·게임 연결은 미승인'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
if before!=after:raise RuntimeError('입력이 변경되었습니다')
print('OV_MM24_PITCH_TRIAL',out)
