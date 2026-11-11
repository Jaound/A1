"""Run in a real Editor session: -ExecCmds="py <absolute script path>".
Tests controller actions inside PIE, not OS mouse/keyboard delivery. Leaves PIE paused.
"""
import unreal, time, json, os, traceback
root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
os.makedirs(os.path.join(root,'Validation'), exist_ok=True)
report={'checks':[], 'kind':'Editor PIE API integration, not physical input', 'errors':[]}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state={'phase':0,'start':time.monotonic()}
def check(name, condition, detail=''):
    report['checks'].append({'name':name,'passed':bool(condition),'detail':str(detail)})
    unreal.log('OV VALIDATE: '+name+' '+str(condition)+' '+str(detail))
    if not condition: raise AssertionError(name)
def prop(obj,name): return obj.get_editor_property(name)
def near(a,b,tolerance=.001):
    fields=('x','y','z') if isinstance(a,unreal.Vector) else ('pitch','yaw','roll')
    return all(abs(getattr(a,k)-getattr(b,k))<=tolerance for k in fields)
def save():
    report['passed']=bool(report['checks']) and all(x['passed'] for x in report['checks']) and not report['errors']
    with open(os.path.join(root,'Validation','PIE.json'),'w',encoding='utf-8') as f:json.dump(report,f,indent=2)
def tick(dt):
    try:
        if time.monotonic()-state['start']>100:raise TimeoutError('PIE validation timeout')
        if state['phase']==0:
            check('Editor map loaded',editor.get_editor_world().get_name()=='ControlRange')
            level.editor_request_begin_play();state['phase']=1;return
        if state['phase']==1:
            world=editor.get_game_world()
            if not world:return
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not pc:return
            ships=prop(pc,'ships')
            if len(ships)!=3:return
            state.update(pc=pc,ships=ships,world=world)
            check('Real PIE world',level.is_in_play_in_editor() and 'UEDPIE' in world.get_path_name(),world.get_path_name())
            check('Two friendly + one enemy',sum(prop(s,'friendly') for s in ships)==2)
            pc.action('Pause')
            state['pos']=ships[0].get_actor_location();state['sim']=prop(pc,'simulation_seconds');state['at']=time.monotonic();state['phase']=2;return
        if state['phase']==2:
            if time.monotonic()-state['at']<1:return
            pc=state['pc']; ships=state['ships']; a,b,target=ships
            check('Pause freezes real-time simulation',near(a.get_actor_location(),state['pos']) and prop(pc,'simulation_seconds')==state['sim'])
            pc.action('Ship2'); check('Select while paused',prop(pc,'selected')==b and prop(b,'selected'))
            camera=prop(pc,'range_camera');old=camera.get_actor_location()
            pc.set_editor_property('camera_focus',unreal.Vector(500,300,400));pc.update_camera()
            check('Camera pan while paused',not near(camera.get_actor_location(),old,1))
            old_rot=camera.get_actor_rotation()
            pc.set_editor_property('camera_yaw',20);pc.set_editor_property('camera_pitch',-35);pc.update_camera()
            check('Camera orbit while paused',not near(camera.get_actor_rotation(),old_rot,1))
            old=camera.get_actor_location();pc.set_editor_property('camera_distance',4500);pc.update_camera()
            check('Camera zoom while paused',not near(camera.get_actor_location(),old,1))
            pc.action('Move');pc.set_editor_property('preview',unreal.Vector(-200,1200,1400));pc.action('Confirm')
            check('XYZ order while paused',prop(b,'has_destination') and prop(b,'destination').z==1400 and prop(b,'manual'))
            still=b.get_actor_location();pc.step_simulation(10);check('Paused order does not move ship',near(still,b.get_actor_location()))
            pc.action('Pause');pc.action('Speed1')
            for i in range(1200):pc.step_simulation(1/60)
            check('XYZ arrival and stop',not prop(b,'has_destination') and near(b.get_actor_location(),unreal.Vector(-200,1200,1400),.1),b.get_actor_location())
            pc.step_simulation(1);check('Manual persists after arrival',prop(b,'manual') and near(b.get_actor_location(),unreal.Vector(-200,1200,1400),.1))
            pc.action('Auto');rot=b.get_actor_rotation();pc.step_simulation(.1)
            check('Auto heading changes',not prop(b,'manual') and not near(b.get_actor_rotation(),rot,.01))
            pc.action('Manual');check('Explicit manual restored',prop(b,'manual'))
            distances=[]
            pc.action('Ship1')
            for speed in [1,2,3]:
                a.set_actor_location(unreal.Vector(0,0,300),False,False);a.set_actor_rotation(unreal.Rotator(0,0,0),False)
                pc.action('Move');pc.set_editor_property('preview',unreal.Vector(5000,0,300));pc.action('Confirm');pc.action('Speed'+str(speed))
                before=prop(pc,'simulation_seconds')
                for i in range(60):pc.step_simulation(1/60)
                distances.append(a.get_actor_location().x)
                check('Simulation clock '+str(speed)+'x',abs(prop(pc,'simulation_seconds')-before-speed)<.001)
            check('1/2/3x actual travel ratio',abs(distances[1]/distances[0]-2)<.01 and abs(distances[2]/distances[0]-3)<.01,distances)
            # Arrange an elevated active order for the final visual inspection.
            a.set_actor_location(unreal.Vector(-1000,-650,300),False,False)
            b.set_actor_location(unreal.Vector(-800,650,300),False,False)
            pc.action('Speed1');pc.action('Pause');pc.set_editor_property('camera_focus',unreal.Vector(0,0,0));pc.set_editor_property('camera_yaw',-45);pc.set_editor_property('camera_pitch',-48);pc.set_editor_property('camera_distance',6500);pc.update_camera()
            pc.action('Move');pc.set_editor_property('preview',unreal.Vector(800,-400,1600));pc.action('Confirm')
            state['pos']=a.get_actor_location();state['sim']=prop(pc,'simulation_seconds')
            pc.action('Pause')
            state['at']=time.monotonic();state['phase']=3;return
        if state['phase']==3:
            if time.monotonic()-state['at']<2:return
            pc=state['pc'];a=state['ships'][0]
            check('Real PIE frames advance movement',not near(a.get_actor_location(),state['pos'],1) and prop(pc,'simulation_seconds')>state['sim']+0.1,a.get_actor_location())
            pc.action('Pause')
            pc.action('Move');pc.action('Up')
            check('Altitude control works while paused',prop(pc,'placing') and abs(prop(pc,'preview').z-a.get_actor_location().z-250)<.01)
            state['at']=time.monotonic();state['phase']=4;return
        if state['phase']==4:
            if time.monotonic()-state['at']<2:return
            unreal.SystemLibrary.execute_console_command(state['world'],'Shot SHOWUI',state['pc'])
            save();unreal.unregister_slate_post_tick_callback(state['handle']);unreal.log('OV VALIDATE: COMPLETE')
    except Exception:
        report['errors'].append(traceback.format_exc());save();unreal.log_error(report['errors'][-1]);unreal.unregister_slate_post_tick_callback(state['handle'])
state['handle']=unreal.register_slate_post_tick_callback(tick)
