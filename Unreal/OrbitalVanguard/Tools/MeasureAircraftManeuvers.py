"""수정 전/후 동일 조건의 함재기 대함 기동 API 측정. 실제 입력/렌더 성능 검사가 아니다.

OV_AIRCRAFT_LABEL=Before/After. 기하 크기만 6/13/22로 비교하며 모든 시험 표적은
별도 내구 시험체(선체/쉴드 100만)다. 최종 밸런스나 함종별 실전 승률로 해석하지 않는다.
"""
import collections, json, math, os, statistics, time, traceback, unreal
os.makedirs(os.path.join(unreal.Paths.project_saved_dir(), 'Validation'), exist_ok=True)

label = os.environ.get('OV_AIRCRAFT_LABEL', 'After')
report = {'종류': '동일 시드 PIE API 기동 비교, OS 입력·렌더 성능과 분리', '단계': label,
          '시드': 260112, '시험값': 'F7 항모 40기, PD/적 사격/적 관제 정지, 대상 내구 100만; 속도/화력 보정 없음',
          'cases': [], 'errors': []}
state = {'phase': 0, 'at': time.monotonic()}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def p(obj, key): return obj.get_editor_property(key)
def scene(pc):
    (getattr(pc, 'start_phase2c_range', None) or getattr(pc, 'start_phase2_c_range'))(2)
def summary(values):
    values = sorted(values)
    return {'평균_m': statistics.mean(values)/100, 'p95_m': values[min(len(values)-1, int(len(values)*.95))]/100,
            '최대_m': values[-1]/100, '표본': len(values)} if values else {}
def run(pc, scale, moving, speed, multi=False):
    if multi: (getattr(pc, 'start_phase2c_stress', None) or getattr(pc, 'start_phase2_c_stress'))(1)
    else: scene(pc)
    ships=list(p(pc, 'ships')); own,target=ships[0],ships[2]
    attackers=[own,ships[3]] if multi else [own]
    fleet=p(pc, 'carrier'); combat=p(pc, 'combat'); p(pc, 'point_defense').disable()
    for ship in ships:
        ship.set_automation(False,False); combat.cancel_attack_order(ship)
        if p(p(ship,'systems'),'faction_id') != 0:
            for slot in p(p(p(ship,'systems'),'layout'),'slots'):
                if p(slot,'facility') == unreal.OVFacilityType.FIGHTER_CONTROL:
                    p(ship,'systems').debug_set_crew(p(slot,'slot_id'),0)
    if multi:
        ships[3].set_actor_location(unreal.Vector(-24000,4500,2000),False,False)
        ships[4].set_actor_location(unreal.Vector(1000000,1000000,2000),False,False)
    systems=p(target,'systems');p(systems,'layout').set_editor_property('max_hull_hp',1000000.)
    tuning=p(systems,'tuning');tuning.set_editor_property('shield_base',1000000.);systems.set_editor_property('tuning',tuning)
    systems.reset_state()
    for slot in p(p(p(target,'systems'),'layout'),'slots'):
        if p(slot,'facility') == unreal.OVFacilityType.FIGHTER_CONTROL:
            p(target,'systems').debug_set_crew(p(slot,'slot_id'),0)
    target.set_actor_scale3d(unreal.Vector(scale,scale,scale))
    own.set_actor_location(unreal.Vector(-24000,-3500,2000),False,False)
    target.set_actor_location(unreal.Vector(24000,0,2000),False,False)
    if moving: target.set_destination(unreal.Vector(65000,28000,7000))
    for attacker in attackers: combat.set_target(attacker,target); fleet.launch(attacker)
    pc.action('Speed'+str(speed)); pc.action('Pause')
    transition=[]; normal=[]; cycles=collections.defaultdict(list); lastshots={}; hits=set(); targethits=set(); shots_before=0; hits_before=0; targethits_before=0
    targethp_before=p(systems,'hull_hp')+p(systems,'shield')
    maxstep=0.; previous={}; firstshot=None; states=collections.Counter()
    for i in range(400):
        pc.step_simulation(.1/speed)
        now=(i+1)*.1
        for craft in p(fleet,'aircraft'):
            if p(craft,'carrier') not in attackers or p(craft,'state') != unreal.OVAircraftState.SORTIE: continue
            cid=p(craft,'aircraft_id'); pos=p(craft,'position'); count=p(craft,'shots_fired')
            distance=(pos-target.get_actor_location()).length()
            (normal if now>=8 else transition).append(distance)
            if cid in previous: maxstep=max(maxstep,(pos-previous[cid]).length())
            previous[cid]=pos
            if count>lastshots.get(cid,0): cycles[cid].append(now); firstshot=now if firstshot is None else firstshot
            lastshots[cid]=count
            try: states[str(p(craft,'maneuver'))]+=1
            except Exception: states[str(p(craft,'leg'))]+=1
        for event in p(fleet,'recent_events'):
            if p(event,'faction_id') == 0 and p(event,'hit'):
                hits.add(p(event,'bolt_id'))
                if str(p(event,'hit_id')) == str(p(target,'ship_name')): targethits.add(p(event,'bolt_id'))
        if i==78:
            shots_before=sum(lastshots.values()); hits_before=len(hits); targethits_before=len(targethits)
            targethp_before=p(systems,'hull_hp')+p(systems,'shield')
    pc.action('Pause')
    periods=[b-a for times in cycles.values() for a,b in zip(times,times[1:]) if a>=8]
    return {'대상_배율':scale,'이동_선회':moving,'배속':speed,'공격항모수':len(attackers),'전환구간_0_8초':summary(transition),'공격구간_8_40초':summary(normal),
            '첫발_게임초':firstshot,'전체_발사':sum(lastshots.values()),'전체_명중':len(hits),
            '공격구간_발사':sum(lastshots.values())-shots_before,'공격구간_명중':len(hits)-hits_before,
            '공격구간_이론명중피해초당':(len(hits)-hits_before)*p(p(fleet,'profile'),'damage')/32,
            '명중환산_주의':'이론명중피해는 타 기체/함선과의 충돌도 포함하며 표적 실효 DPS가 아니다',
            '공격구간_표적만명중':len(targethits)-targethits_before,
            '공격구간_표적실제HP쉴드감소초당':(targethp_before-p(systems,'hull_hp')-p(systems,'shield'))/32,
            '사격간격_평균초':statistics.mean(periods) if periods else None,
            '사격간격_p95초':sorted(periods)[min(len(periods)-1,int(len(periods)*.95))] if periods else None,
            '100ms_최대이동_cm':maxstep,'기동상태_표본':dict(states)}
def tick(dt):
    try:
        if state['phase']==0: level.editor_request_begin_play();state['phase']=1;return
        world=editor.get_game_world()
        if not world:return
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        if not pc or len(p(pc,'ships'))<3:return
        for scale,moving,speed,multi in [(6,False,1,False),(13,False,1,False),(22,False,1,False),(13,True,1,False),(13,True,2,False),(13,True,3,False),(13,True,1,True)]:
            result=run(pc,scale,moving,speed,multi);report['cases'].append(result);unreal.log('함재기 기동 측정 '+str(result))
        report['passed']=True
        unreal.unregister_slate_post_tick_callback(state['handle'])
        out=os.path.join(unreal.Paths.project_saved_dir(),'Validation','AircraftManeuvers'+label+'.json')
        with open(out,'w',encoding='utf-8') as stream:json.dump(report,stream,ensure_ascii=False,indent=2)
        unreal.log('함재기 기동 측정 완료 '+out);unreal.SystemLibrary.quit_editor()
    except Exception:
        report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1])
        out=os.path.join(unreal.Paths.project_saved_dir(),'Validation','AircraftManeuvers'+label+'.json')
        with open(out,'w',encoding='utf-8') as stream:json.dump(report,stream,ensure_ascii=False,indent=2)
        unreal.unregister_slate_post_tick_callback(state['handle']);unreal.SystemLibrary.quit_editor()
state['handle']=unreal.register_slate_post_tick_callback(tick)
