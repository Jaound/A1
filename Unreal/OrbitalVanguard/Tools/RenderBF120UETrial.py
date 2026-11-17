"""BF120 세 부품의 UE 격리 좌우·상하 피벗과 렌더를 확인한다.

저장되지 않은 빈 레벨이며 게임 연결·원본 품질 승인·직접 입력이 아니다.
"""
import json,math,os,time,traceback
from pathlib import Path
import unreal
tag=os.environ.get('OV_BF120_RENDER_TAG','tri_pivot_v1')
if not tag.replace('_','').replace('-','').isalnum():raise ValueError('Invalid render tag')
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'Validation/BF120UERender'/tag
out.mkdir(parents=True,exist_ok=False)
report={'passed':None,'checks':[],'errors':[],'physical_input':False,'gameplay':False,'scope':'unsaved isolated UE three-part pivot and render; no art/gameplay approval'}
state={'started':time.monotonic(),'stage':'setup'}
def check(name,value,detail=''):
 report['checks'].append({'id':name,'passed':bool(value),'detail':str(detail)})
 if not value:raise AssertionError(name+': '+str(detail))
def finish():
 report['passed']=not report['errors'] and bool(report['checks']);(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(state['handle']);unreal.SystemLibrary.quit_editor()
def tick(dt):
 try:
  if time.monotonic()-state['started']>120:raise TimeoutError('render timeout')
  if state['stage']=='setup':
   world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False);check('NEW-UNSAVED-WORLD',world is not None);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
   clearance=float(os.environ.get('OV_BF120_CLEARANCE_CM','0'));bottom=os.environ.get('OV_BF120_BOTTOM')=='1'
   check('CLEARANCE-BOUNDED',0<=clearance<=5,clearance)
   yaw_pivot=unreal.Vector(99.5,2.5,-42+clearance);pitch_pivot=unreal.Vector(8,0,49+clearance)
   yaw=actors.spawn_actor_from_class(unreal.TargetPoint,yaw_pivot);pitch=actors.spawn_actor_from_class(unreal.TargetPoint,pitch_pivot)
   pitch.attach_to_actor(yaw,unreal.Name('None'),unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
   check('PITCH-PARENT',pitch.get_attach_parent_actor()==yaw);check('INITIAL-PITCH-PIVOT',(pitch.get_actor_location()-pitch_pivot).length()<.01)
   objects={}
   for part in ('BaseTrial','YawBody','PitchTrial'):
    mesh=unreal.load_asset('/Game/Presentation/BF120Trial/SM_BF120_'+part);check('MESH-'+part,isinstance(mesh,unreal.StaticMesh))
    initial=unreal.Vector(0,0,0 if part=='BaseTrial' else clearance)
    obj=actors.spawn_actor_from_class(unreal.StaticMeshActor,initial);obj.static_mesh_component.set_static_mesh(mesh);obj.set_actor_enable_collision(False)
    if part!='BaseTrial':
     obj.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE);parent=yaw if part=='YawBody' else pitch
     obj.attach_to_actor(parent,unreal.Name('None'),unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
     check('ATTACH-'+part,obj.get_attach_parent_actor()==parent);check('INITIAL-MESH-ORIGIN-'+part,(obj.get_actor_location()-initial).length()<.01)
    objects[part]=obj
   mount=None
   if bottom:
    mount=actors.spawn_actor_from_class(unreal.TargetPoint,unreal.Vector(0,0,0))
    for child in (yaw,objects['BaseTrial']):
     if child==objects['BaseTrial']:child.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
     child.attach_to_actor(mount,unreal.Name('None'),unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
     check('BOTTOM-PARENT-'+child.get_name(),child.get_attach_parent_actor()==mount)
    mount.set_actor_rotation(unreal.Rotator(180,0,0),False)
   sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,500),unreal.Rotator(-50,-30,0));sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sun.light_component.set_intensity(5)
   fill=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,500),unreal.Rotator(-25,145,0));fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);fill.light_component.set_intensity(4)
   pos=unreal.Vector(-600,700,400);rot=unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(0,0,0));camera=actors.spawn_actor_from_class(unreal.CameraActor,pos,rot);camera.camera_component.set_editor_property('field_of_view',50.0)
   unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).set_level_viewport_camera_info(pos,rot,'')
   angles=((0,0),(0,-15),(45,-15),(-45,15),(0,-90),(0,15)) if bottom else ((0,0),(0,15),(45,15),(-45,-15),(0,90),(0,-15))
   state.update(stage='render',camera=camera,yaw=yaw,pitch=pitch,objects=objects,yaw_pivot=yaw_pivot,pitch_pivot=pitch_pivot,mount=mount,bottom=bottom,angles=angles,index=0,at=time.monotonic());return
  if state['stage']=='render' and time.monotonic()-state['at']>3:
   yaw_angle,pitch_angle=state['angles'][state['index']];bottom=state['bottom'];source_yaw=180-yaw_angle if bottom else yaw_angle+180;source_pitch=pitch_angle if bottom else -pitch_angle
   state['yaw'].set_actor_relative_rotation(unreal.Rotator(0,0,source_yaw),False,False);state['pitch'].set_actor_relative_rotation(unreal.Rotator(0,source_pitch,0),False,False)
   yaw_relative=state['yaw'].get_editor_property('root_component').get_editor_property('relative_rotation')
   check('YAW-ROTATION-'+str(state['index']),abs((yaw_relative.yaw-source_yaw+180)%360-180)<.1)
   body_forward=unreal.MathLibrary.get_forward_vector(state['objects']['YawBody'].get_actor_rotation());yaw_forward=unreal.MathLibrary.get_forward_vector(state['yaw'].get_actor_rotation())
   check('YAW-BODY-ROTATION-'+str(state['index']),(body_forward-yaw_forward).length()<.001)
   relative=state['pitch'].get_editor_property('root_component').get_editor_property('relative_rotation')
   check('PITCH-RELATIVE-'+str(state['index']),abs(relative.pitch-source_pitch)<.1)
   mesh=state['objects']['PitchTrial'];transform=mesh.get_actor_transform()
   tip=unreal.MathLibrary.transform_location(transform,unreal.Vector(-242.155,0,49));rear=unreal.MathLibrary.transform_location(transform,unreal.Vector(-20,0,49))
   direction=tip-rear;direction=direction/direction.length();expected_direction=unreal.MathLibrary.get_forward_vector(unreal.Rotator(0,pitch_angle,yaw_angle))
   check('BORE-DIRECTION-'+str(state['index']),direction.x*expected_direction.x+direction.y*expected_direction.y+direction.z*expected_direction.z>.999)
   a=math.radians(source_yaw);p=state['yaw_pivot'];q=state['pitch_pivot'];dx=q.x-p.x;dy=q.y-p.y
   expected=unreal.Vector(p.x+dx*math.cos(a)-dy*math.sin(a),p.y+dx*math.sin(a)+dy*math.cos(a),q.z)
   if bottom:expected=unreal.MathLibrary.transform_location(state['mount'].get_actor_transform(),expected)
   check('PITCH-PIVOT-FOLLOWS-YAW-'+str(state['index']),(state['pitch'].get_actor_location()-expected).length()<.1)
   path=str(out/('BF120_yaw%+03d_pitch%+03d.png'%(yaw_angle,pitch_angle)));check('NO-OVERWRITE-'+str(state['index']),not Path(path).exists())
   task=unreal.AutomationLibrary.take_high_res_screenshot(1024,1024,path,camera=state['camera'],delay=1.0);check('SCREENSHOT-TASK-'+str(state['index']),task is not None)
   state.update(stage='wait',path=path,at=time.monotonic());return
  if state['stage']=='wait' and time.monotonic()-state['at']>3:
   path=Path(state['path'])
   if not path.exists() or path.stat().st_size<=1000:return
   check('RENDER-FILE-'+str(state['index']),True,path.stat().st_size);report.setdefault('images',[]).append(str(path));state['index']+=1
   if state['index']==len(state['angles']):finish()
   else:state.update(stage='render',at=time.monotonic())
 except Exception:
  report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
