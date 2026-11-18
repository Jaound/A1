"""기존 2C PIE API 검사에 도면 일반 슬롯의 실제 모듈 교체를 추가한다."""
import os, time, traceback
import unreal

tool=os.path.join(unreal.Paths.project_dir(),'Tools','ValidatePhase2C.py')
with open(tool,encoding='utf8') as file: source=file.read()
if source.count('"Phase2C.json"') != 1: raise RuntimeError('기존 보고서 경로 확인 필요')
run=os.environ.get('OV_MARKED_MODULE_RUN','first')
if not run.replace('_','').isalnum(): raise ValueError('잘못된 검사 ID')
output='MarkedModule_2C_'+run+'.json'
target=os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),'Validation',output)
if os.path.exists(target): raise FileExistsError('기존 검사 보고서는 덮어쓰지 않습니다: '+target)
source=source.replace('"Phase2C.json"','"'+output+'"')
exec(compile(source,tool,'exec'),globals())
trial={'started':time.monotonic()}

def validate(dt):
    try:
        if 'passed' not in report:
            if time.monotonic()-trial['started']>360: raise RuntimeError('기존 2C 검사 대기 시간 초과')
            return
        if not report['passed']: raise RuntimeError('기존 2C 검사 실패')
        pc=state['pc']
        if not pc.start_calibration_range(1): raise RuntimeError('F6 교정 장면 생성 실패')
        combat=p(pc,'combat'); profiles=p(p(combat,'settings'),'weapons')
        candidates=[]
        for ship in p(pc,'ships'):
            systems=p(ship,'systems'); layout=p(systems,'layout')
            for mount in p(layout,'weapon_mounts'):
                if str(p(mount,'authored_slot_purpose'))!='General': continue
                for profile in profiles:
                    if (p(profile,'family')!=p(mount,'type') and p(profile,'tier')==round(p(mount,'tier'))
                            and p(profile,'mount_max_hp')>0):
                        candidates.append((ship,mount,profile)); break
        check('MARKED-MODULE-CANDIDATE',bool(candidates),len(candidates))
        ship,mount,profile=candidates[0];systems=p(ship,'systems');layout=p(systems,'layout')
        mount_id=p(mount,'weapon_id');old_type=p(mount,'type');new_type=p(profile,'family')
        old_health=next(p(h,'hp') for h in p(systems,'weapon_mount_states') if p(h,'weapon_mount_id')==mount_id)
        old_counts=(p(layout,'beam_slots'),p(layout,'cannon_slots'),p(layout,'missile_slots'))
        result=combat.set_weapon_profile(ship,mount_id,p(profile,'profile_id'))
        new_layout=p(systems,'layout');updated=next(m for m in p(new_layout,'weapon_mounts') if p(m,'weapon_id')==mount_id)
        health=next(p(h,'hp') for h in p(systems,'weapon_mount_states') if p(h,'weapon_mount_id')==mount_id)
        new_counts=(p(new_layout,'beam_slots'),p(new_layout,'cannon_slots'),p(new_layout,'missile_slots'))
        runtime=next(m for m in p(combat.get_ship_state(ship),'mounts') if p(m,'mount_id')==mount_id)
        check('MARKED-MODULE-ACCEPTED',result)
        check('MARKED-MODULE-NEW-FAMILY',p(updated,'type')==new_type and p(p(runtime,'profile'),'family')==new_type,
              f'{old_type} -> {new_type}')
        check('MARKED-MODULE-NO-FREE-REPAIR',health<=old_health and health<=p(updated,'max_hp'),f'{old_health} -> {health}')
        check('MARKED-MODULE-SUPPLY-COUNTS',sum(old_counts)==sum(new_counts) and old_counts!=new_counts,
              f'{old_counts} -> {new_counts}')
        report['scope']+='; 도면 일반 슬롯 실제 계열 교체 API, 직접 입력 아님'
        save();unreal.unregister_slate_post_tick_callback(trial['handle']);unreal.SystemLibrary.quit_editor()
    except Exception:
        report['errors'].append(traceback.format_exc());save();unreal.log_error(report['errors'][-1])
        unreal.unregister_slate_post_tick_callback(trial['handle']);unreal.SystemLibrary.quit_editor()

trial['handle']=unreal.register_slate_post_tick_callback(validate)
