"""미사일 원본 상자·받침과 새 시험 지지대를 조합한다. 게임용 최종 자산이 아니다."""
import argparse,hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils import Matrix,Vector

p=argparse.ArgumentParser();p.add_argument('--prefix',required=True);p.add_argument('--output',required=True)
p.add_argument('--pivot-z',type=float,required=True);p.add_argument('--frame-x',type=float,required=True)
p.add_argument('--plate-x',type=float,required=True);p.add_argument('--plate-y',type=float,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);source,out=Path(bpy.data.filepath).resolve(),Path(a.output).resolve()
if not source.is_file() or out.exists() or not .5<a.pivot_z<2 or not 1<a.frame_x<2:raise ValueError('입력 확인 필요')
def sha():
    with source.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();base=bpy.data.objects.get(a.prefix+'_BaseTrial');body=bpy.data.objects.get(a.prefix+'_YawBody');pod=bpy.data.objects.get(a.prefix+'_PitchTrial')
if not base or not body or not pod:raise RuntimeError('분리 후보 누락')
bpy.context.view_layer.update();world=pod.matrix_world.copy();pod.parent=None;pod.matrix_world=world
old=pod.matrix_world.translation.copy();pivot=Vector((0,0,a.pivot_z))
pod.data.transform(Matrix.Translation(old-pivot));pod.location=pivot
bm=bmesh.new();bm.from_mesh(pod.data)
caps=[f for f in bm.faces if f.material_index<len(pod.data.materials) and pod.data.materials[f.material_index]
      and pod.data.materials[f.material_index].name.startswith('MissileTrialCutCap')]
if not caps:raise RuntimeError('시험용 절단 덮개 누락')
cap_count=len(caps);bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY');bm.to_mesh(pod.data);bm.free()
bpy.data.objects.remove(body,do_unlink=True)
material=bpy.data.materials.new('ReconstructedTrialFrame');material.diffuse_color=(.21,.23,.25,1)
def cube(name,location,scale,parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=location);obj=bpy.context.object;obj.name=name;obj.dimensions=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);obj.data.materials.append(material)
    if parent:
        matrix=obj.matrix_world.copy();obj.parent=parent;obj.matrix_world=matrix
    return obj
# The new cover is separate: it hides the cut but is not original artwork.
plate=cube(a.prefix+'_TrialPlate',Vector((0,0,-.18)),Vector((a.plate_x,a.plate_y,.08)),pod)
base_top=base.matrix_world.translation.z
for side in (-1,1):
    height=a.pivot_z-base_top
    cube(a.prefix+'_TrialPost_'+str(side),Vector((side*a.frame_x,0,(base_top+a.pivot_z)/2)),Vector((.20,.28,height)))
bpy.ops.mesh.primitive_cylinder_add(vertices=48,radius=.15,depth=2*a.frame_x+.15,location=pivot,rotation=(0,math.pi/2,0))
axle=bpy.context.object;axle.name=a.prefix+'_TrialAxle';axle.data.materials.append(material)
out.mkdir(parents=True)
for angle in (-90,-60,-30,-15,0,15,30,60,90):
    pod.rotation_euler.x=math.radians(angle);bpy.context.view_layer.update()
    bpy.context.scene.render.filepath=str(out/('pitch_%+04d.png'%angle));bpy.ops.render.render(write_still=True)
pod.rotation_euler.x=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/(a.prefix+'_FrameTrial.blend')))
after=sha();(out/'report.json').write_text(json.dumps({'source':str(source),'source_before':before,'source_after':after,
    'pivot':list(pivot),'frame_x':a.frame_x,'generated_posts':2,'generated_axle':True,'generated_cover':True,
    'removed_trial_cap_faces':cap_count,'scope':'원본 상자·받침과 대체 지지대의 시각 시험. 실제 원본 관절·전장·간섭 미검증'},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
if before!=after:raise RuntimeError('원본 변경됨')
print('OV_MISSILE_FRAME_TRIAL',out)
