"""실제 항공기 상태와 원본 모델 인스턴스의 대응 검사. OS 직접 입력이 아니다."""
import json, os, time, traceback, unreal

out=os.path.join(unreal.Paths.project_saved_dir(),'Validation','FighterPresentation.json')
report={'passed':None,'kind':'PIE API, 직접 입력 아님','checks':[],'errors':[]}
state={'phase':0,'start':time.monotonic()}
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
def p(obj,key): return obj.get_editor_property(key)
def check(name,value):
    report['checks'].append({'id':name,'passed':bool(value)})
    if not value: raise AssertionError(name)
def finish():
    report['passed']=bool(report['checks']) and all(x['passed'] for x in report['checks']) and not report['errors']
    os.makedirs(os.path.dirname(out),exist_ok=True)
    with open(out,'w',encoding='utf8') as f: json.dump(report,f,ensure_ascii=False,indent=2)
    unreal.unregister_slate_post_tick_callback(state['handle'])
def tick(_):
    try:
        if time.monotonic()-state['start']>120: raise RuntimeError('시간 초과')
        if state['phase']==0: level.editor_request_begin_play();state['phase']=1;return
        world=editor.get_game_world();pc=unreal.GameplayStatics.get_player_controller(world,0) if world else None
        if not pc:return
        check('FIGHTER-SCENE',pc.start_phase2c_range(2))
        if not p(pc,'simulation_paused'):pc.action('Pause')
        wing=p(pc,'carrier');own=p(pc,'ships')[0]
        before=[(p(a,'aircraft_id'),p(a,'hp'),p(a,'state')) for a in p(wing,'aircraft')]
        check('FIGHTER-HAS-RECORDS',bool(before))
        check('FIGHTER-LAUNCH',wing.launch(own)>0)
        live=[a for a in p(wing,'aircraft') if p(a,'state') not in (unreal.OVAircraftState.STORED,unreal.OVAircraftState.WRECKED)]
        after=[(p(a,'aircraft_id'),p(a,'hp'),p(a,'state')) for a in p(wing,'aircraft')]
        pc.update_aircraft_presentation()
        friendly=p(pc,'friendly_aircraft_meshes');enemy=p(pc,'enemy_aircraft_meshes')
        check('FIGHTER-INSTANCE-COMPONENTS',bool(friendly) and bool(enemy))
        check('FIGHTER-REAL-SOURCE-MESH',p(friendly,'static_mesh').get_path_name()=='/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull')
        check('FIGHTER-INSTANCE-COUNTS',friendly.get_instance_count()==sum(p(a,'faction_id')==0 for a in live) and enemy.get_instance_count()==sum(p(a,'faction_id')!=0 for a in live))
        first=next(a for a in live if p(a,'faction_id')==0)
        transform=friendly.get_instance_transform(0,True)
        if isinstance(transform,tuple):transform=transform[-1]
        check('FIGHTER-WORLD-POSITION',(transform.translation-p(first,'position')).length()<.1)
        pc.update_aircraft_presentation();pc.update_aircraft_presentation()
        check('FIGHTER-RENDER-DOES-NOT-CHANGE-RECORDS',after==[(p(a,'aircraft_id'),p(a,'hp'),p(a,'state')) for a in p(wing,'aircraft')])
        check('FIGHTER-RENDER-DOES-NOT-DUPLICATE',friendly.get_instance_count()+enemy.get_instance_count()==len(live))
        check('FIGHTER-IDS-RETAINED',[x[0] for x in before]==[x[0] for x in after])
        pc.start_calibration_range(1);pc.update_aircraft_presentation()
        check('FIGHTER-SCENE-RESET-CLEARS',friendly.get_instance_count()==0 and enemy.get_instance_count()==0)
        finish()
    except Exception:
        report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
