"""원본 상부를 절단하지 않는 가중 관절 시험. 새 복사본만 생성한다."""
import argparse,hashlib,json,math,sys
from pathlib import Path
import bpy
from mathutils import Vector

p=argparse.ArgumentParser();p.add_argument('--prefix',required=True);p.add_argument('--output',required=True)
p.add_argument('--pivot-y',type=float,required=True);p.add_argument('--pivot-z',type=float,required=True)
p.add_argument('--blend-low',type=float,required=True);p.add_argument('--blend-high',type=float,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);source,out=Path(bpy.data.filepath).resolve(),Path(a.output).resolve()
if not source.is_file() or out.exists() or a.blend_low>=a.blend_high:raise RuntimeError('입력 확인 필요')
def sha():
    with source.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();body=bpy.data.objects.get(a.prefix+'_YawTrial');base=bpy.data.objects.get(a.prefix+'_BaseTrial')
if not body or not base or body.type!='MESH':raise RuntimeError('원본 회전 부품이 없습니다')
origin=body.matrix_world.translation.copy();pivot=Vector((0,a.pivot_y,a.pivot_z));out.mkdir(parents=True)
armdata=bpy.data.armatures.new(a.prefix+'_TrialArmature');arm=bpy.data.objects.new(a.prefix+'_TrialArmature',armdata)
bpy.context.scene.collection.objects.link(arm);arm.location=origin
bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm
bpy.ops.object.mode_set(mode='EDIT')
root=armdata.edit_bones.new('MountRoot');root.head=(0,0,0);root.tail=(1,0,0)
pitch=armdata.edit_bones.new('LauncherPitch');pitch.head=pivot-origin;pitch.tail=pitch.head+Vector((1,0,0));pitch.parent=root
bpy.ops.object.mode_set(mode='OBJECT')
root_group=body.vertex_groups.new(name='MountRoot');pitch_group=body.vertex_groups.new(name='LauncherPitch')
bins=[[] for _ in range(101)]
for v in body.data.vertices:
    z=(body.matrix_world@v.co).z; weight=min(1.,max(0.,(z-a.blend_low)/(a.blend_high-a.blend_low)))
    bins[round(weight*100)].append(v.index)
for i,indices in enumerate(bins):
    if not indices:continue
    w=i/100
    if w<1:root_group.add(indices,1-w,'REPLACE')
    if w>0:pitch_group.add(indices,w,'REPLACE')
mod=body.modifiers.new('TrialArticulation','ARMATURE');mod.object=arm
bone=arm.pose.bones['LauncherPitch'];bone.rotation_mode='XYZ'
for angle in (-90,-60,-30,-15,0,15,30,60,90):
    bone.rotation_euler.y=math.radians(angle);bpy.context.view_layer.update()
    bpy.context.scene.render.filepath=str(out/('pitch_%+04d.png'%angle));bpy.ops.render.render(write_still=True)
bone.rotation_euler.y=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/(a.prefix+'_SkinTrial.blend')))
after=sha();(out/'report.json').write_text(json.dumps({'source':str(source),'source_before':before,'source_after':after,
    'pivot':list(pivot),'blend_low':a.blend_low,'blend_high':a.blend_high,'weight_bins':[len(x) for x in bins],
    'status':'원본 가중 변형 시험. 축·교차·텍스처·UE와 발사 구동 미검증'},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
if before!=after:raise RuntimeError('원본 변경됨')
print('OV_MISSILE_SKIN_TRIAL',out)
