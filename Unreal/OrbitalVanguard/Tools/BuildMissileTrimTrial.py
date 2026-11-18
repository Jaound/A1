"""미사일 접합부를 좁힌 실험 복사본. 원본과 게임 자산은 변경하지 않는다."""
import argparse, hashlib, json, math, sys
from pathlib import Path
import bmesh, bpy
from mathutils import Vector

p=argparse.ArgumentParser();p.add_argument('--prefix',required=True);p.add_argument('--output',required=True)
p.add_argument('--half-x',type=float,required=True);p.add_argument('--half-y',type=float,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);source,out=Path(bpy.data.filepath).resolve(),Path(a.output).resolve()
if not source.is_file() or out.exists() or not .3 <= a.half_x <= 1.5 or not .3 <= a.half_y <= 1.5:raise RuntimeError('입력 확인 필요')
def sha():
    with source.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();body=bpy.data.objects.get(a.prefix+'_YawBody');launcher=bpy.data.objects.get(a.prefix+'_PitchTrial')
if not body or not launcher or launcher.parent!=body:raise RuntimeError('후보 부품 누락')
bm=bmesh.new();bm.from_mesh(body.data);bm.transform(body.matrix_world)
cuts=[(Vector((a.half_x,0,0)),Vector((1,0,0))), (Vector((-a.half_x,0,0)),Vector((-1,0,0))),
      (Vector((0,a.half_y,0)),Vector((0,1,0))), (Vector((0,-a.half_y,0)),Vector((0,-1,0)))]
for point,normal in cuts:
    result=bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=1e-5,
                                 plane_co=point,plane_no=normal,clear_outer=True,clear_inner=False)
    edges=[e for e in result['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_valid and e.is_boundary]
    if edges:bmesh.ops.holes_fill(bm,edges=edges,sides=0)
bm.transform(body.matrix_world.inverted());topology={'faces':len(bm.faces),'boundary_edges':sum(e.is_boundary for e in bm.edges)}
bm.to_mesh(body.data);bm.free();bpy.context.view_layer.update();pivot=launcher.matrix_world.translation.copy()
bpy.ops.mesh.primitive_cylinder_add(vertices=48,radius=.14,depth=1.35,location=pivot,rotation=(0,math.pi/2,0))
axle=bpy.context.object;axle.name=a.prefix+'_TrialPitchNeck';mat=bpy.data.materials.new('TrialPitchNeckMaterial')
mat.diffuse_color=(.19,.2,.21,1);axle.data.materials.append(mat)
world=axle.matrix_world.copy();axle.parent=launcher;axle.matrix_world=world
out.mkdir(parents=True)
for angle in (-90,-60,-30,-15,0,15,30,60,90):
    launcher.rotation_euler.x=math.radians(angle);bpy.context.view_layer.update()
    bpy.context.scene.render.filepath=str(out/('pitch_%+04d.png'%angle));bpy.ops.render.render(write_still=True)
launcher.rotation_euler.x=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/(a.prefix+'_TrimTrial.blend')))
after=sha();(out/'report.json').write_text(json.dumps({'source':str(source),'source_before':before,'source_after':after,
    'trim_half_x':a.half_x,'trim_half_y':a.half_y,'topology':topology,'generated_axle':True,
    'status':'접합 형상 변경 실험. 표면 교차·원본 외관·포구·전장 미승인'},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
if before!=after:raise RuntimeError('원본 변경됨')
print('OV_MISSILE_TRIM_TRIAL',out)
