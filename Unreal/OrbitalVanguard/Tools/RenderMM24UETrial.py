"""Render MM24 trial assets in an unsaved blank editor level.

This creates no map asset and never saves the level. It does not test gameplay,
art approval, pitch articulation or direct OS input.
"""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

prefix=os.environ.get('OV_TURRET_PREFIX','MM24')
if not prefix.replace('_','').isalnum():raise ValueError('Invalid part prefix')
tag=os.environ.get('OV_TURRET_RENDER_TAG',os.environ.get('OV_MM24_RENDER_TAG',time.strftime('%Y%m%d_%H%M%S')))
if not tag.isalnum() and not all(c.isalnum() or c in '_-' for c in tag):raise ValueError('Invalid output tag')
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/('Validation/'+prefix+'UERender')/tag
out.mkdir(parents=True,exist_ok=False)
report={'passed':None,'checks':[],'errors':[],'scope':'unsaved blank-level UE render only',
        'physical_input':False,'gameplay':False}
state={'started':time.monotonic(),'stage':'setup'}
def check(name,value,detail=''):
    report['checks'].append({'id':name,'passed':bool(value),'detail':str(detail)})
    if not value:raise AssertionError(name+': '+str(detail))
def finish():
    report['passed']=not report['errors'] and bool(report['checks'])
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    unreal.unregister_slate_post_tick_callback(state['handle']);unreal.SystemLibrary.quit_editor()
def tick(_dt):
    try:
        if time.monotonic()-state['started']>100:raise TimeoutError('UE trial render timed out')
        if state['stage']=='setup':
            world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False);check('NEW-UNSAVED-WORLD',world is not None)
            actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            for part in ('Base','Yaw'):
                mesh=unreal.load_asset('/Game/Presentation/'+prefix+'Trial/SM_'+prefix+'_'+part+'Trial')
                check('MESH-'+part,isinstance(mesh,unreal.StaticMesh))
                obj=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,0))
                obj.static_mesh_component.set_static_mesh(mesh);obj.set_actor_enable_collision(False)
                if part=='Yaw':state['yaw_actor']=obj
                bounds=mesh.get_bounding_box()
                report[part]={'min':[float(getattr(bounds.min,k)) for k in ('x','y','z')],
                              'max':[float(getattr(bounds.max,k)) for k in ('x','y','z')]}
            sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,500),unreal.Rotator(-50,-30,0))
            sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sun.light_component.set_intensity(5.0)
            fill=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,500),unreal.Rotator(-25,145,0))
            fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);fill.light_component.set_intensity(4.0)
            pos=unreal.Vector(-500,650,400);rotation=unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(0,0,0))
            camera=actors.spawn_actor_from_class(unreal.CameraActor,pos,rotation)
            camera.camera_component.set_editor_property('field_of_view',50.0)
            levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            levels.set_level_viewport_camera_info(pos,rotation,'')
            state.update(stage='render',camera=camera,at=time.monotonic(),index=0,angles=(0,45,-45));return
        if state['stage']=='render' and time.monotonic()-state['at']>3:
            angle=state['angles'][state['index']];state['yaw_actor'].set_actor_rotation(unreal.Rotator(0,0,angle),False)
            check('YAW-ROTATION-'+str(angle),abs(state['yaw_actor'].get_actor_rotation().yaw-angle)<.1)
            path=str(out/('MM24_%+03d.png'%angle))
            check('NO-SCREENSHOT-OVERWRITE-'+str(angle),not Path(path).exists())
            state['stage']='requesting'
            task=unreal.AutomationLibrary.take_high_res_screenshot(1024,1024,path,camera=state['camera'],delay=1.0)
            check('SCREENSHOT-TASK-'+str(angle),task is not None)
            state.update(stage='wait',task=task,path=path,angle=angle,at=time.monotonic());return
        if state['stage']=='wait' and time.monotonic()-state['at']>3:
            path=Path(state['path'])
            if path.exists() and path.stat().st_size>1000:
                check('RENDER-FILE-'+str(state['angle']),True,path.stat().st_size)
                report.setdefault('images',[]).append({'angle':state['angle'],'path':str(path)})
                state['index']+=1
                if state['index']==len(state['angles']):finish()
                else:state.update(stage='render',at=time.monotonic())
    except Exception:
        report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
