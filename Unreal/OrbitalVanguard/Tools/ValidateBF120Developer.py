"""BF120 개발 도구 명령의 PIE API 검사. 직접 입력은 아니다."""
import json, os, time, traceback
import unreal
saved=unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
out=os.path.join(saved,'Validation','BF120Developer.json');os.makedirs(os.path.dirname(out),exist_ok=True)
png=os.path.join(saved,'Validation','BF120Developer.png').replace('\\','/')
report={'passed':None,'physical_input':False,'checks':[],'errors':[],'image':png,'scope':'개발 도구 명령 API. 원본 적용 완료나 직접 클릭 검증이 아님'}
state={'phase':'start','started':time.monotonic()}; editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
width,height=(int(v) for v in os.environ.get('OV_UI_CAPTURE_RES','1280x720').split('x'))
def p(o,k):return o.get_editor_property(k)
def check(key,ok,detail=''):
 report['checks'].append({'id':key,'passed':bool(ok),'detail':str(detail)})
 if not ok:raise AssertionError(key+': '+str(detail))
def finish():
 if state.get('floating'):report['play_settings_restored']=bool(unreal.OVDevelopmentEditorLibrary.restore_preview_settings())
 report['passed']=bool(report['checks']) and all(c['passed'] for c in report['checks']) and not report['errors']
 with open(out,'w',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
 unreal.unregister_slate_post_tick_callback(state['handle'])
def tick(_):
 try:
  if time.monotonic()-state['started']>90:raise RuntimeError('개발 명령 검사 시간 초과')
  if state['phase']=='start':
   if os.path.isfile(png):os.remove(png)
   check('PREVIEW',unreal.OVDevelopmentEditorLibrary.start_preview_window(width,height));state.update(phase='wait',floating=True);return
  world=editor.get_game_world();pc=unreal.GameplayStatics.get_player_controller(world,0) if world else None
  if not pc or len(p(pc,'ships'))<3:return
  if state['phase']=='wait':
   pc.start_combat_range();ships=list(p(pc,'ships'));source,target=ships[1],ships[2]
   pc.select_ship(source);combat=p(pc,'combat');check('TARGET',combat.set_target(source,target))
   dev=p(pc,'development');systems=p(source,'systems');layout=p(systems,'layout')
   defs=list(p(layout,'weapon_mounts'));gun=next(d for d in defs if p(d,'type')==unreal.OVWeaponType.CANNON)
   mid=p(gun,'weapon_id');beam=next(d for d in defs if p(d,'type')==unreal.OVWeaponType.BEAM)
   snapshot=(p(systems,'hull_hp'),p(systems,'shield'),p(combat,'shot_count'),p(combat,'game_time'),[p(x,'hp') for x in p(systems,'weapon_mount_states')])
   check('DENY_OUTSIDE_DEV',not dev.set_bf120_trial(source,mid,True))
   pc.action('Developer');check('DEV_OPEN',p(pc,'developer_mode'))
   check('DENY_OTHER_WEAPON',not dev.set_bf120_trial(source,p(beam,'weapon_id'),True))
   check('DENY_ENEMY',not dev.set_bf120_trial(target,mid,True))
   check('ENABLE',dev.set_bf120_trial(source,mid,True),p(dev,'last_message'))
   new=p(systems,'layout');d=next(v for v in p(new,'weapon_mounts') if p(v,'weapon_id')==mid)
   check('NEW_ISOLATED_LAYOUT',new!=layout and str(p(d,'presentation_profile'))=='BF120Trial')
   check('ORIGINAL_UNCHANGED',str(p(next(v for v in p(layout,'weapon_mounts') if p(v,'weapon_id')==mid),'presentation_profile'))=='None')
   check('ENABLE_IDEMPOTENT',dev.set_bf120_trial(source,mid,True) and p(systems,'layout')==new)
   check('OTHER_SHIP_UNCHANGED',all(str(p(v,'presentation_profile'))=='None' for v in p(p(p(ships[0],'systems'),'layout'),'weapon_mounts')))
   check('NO_GAMEPLAY_EVENT',snapshot==(p(systems,'hull_hp'),p(systems,'shield'),p(combat,'shot_count'),p(combat,'game_time'),[p(x,'hp') for x in p(systems,'weapon_mount_states')]))
   check('DISABLE',dev.set_bf120_trial(source,mid,False),p(dev,'last_message'))
   d=next(v for v in p(p(systems,'layout'),'weapon_mounts') if p(v,'weapon_id')==mid)
   check('FALLBACK',str(p(d,'presentation_profile'))=='None')
   check('DISABLE_AGAIN_REJECTED',not dev.set_bf120_trial(source,mid,False))
   panel=pc.get_hud().get_development_panel();check('WEAPON_PAGE',panel.open_page(3))
   check('UI_ENABLE_EVENT',panel.debug_activate_bf120_trial_control(True))
   d=next(v for v in p(p(systems,'layout'),'weapon_mounts') if p(v,'weapon_id')==mid)
   check('UI_ENABLE_ACTUAL',str(p(d,'presentation_profile'))=='BF120Trial',p(dev,'last_message'))
   check('UI_DISABLE_EVENT',panel.debug_activate_bf120_trial_control(False))
   d=next(v for v in p(p(systems,'layout'),'weapon_mounts') if p(v,'weapon_id')==mid)
   check('UI_DISABLE_ACTUAL',str(p(d,'presentation_profile'))=='None',p(dev,'last_message'))
   check('UI_ENABLE_AGAIN',panel.debug_activate_bf120_trial_control(True))
   check('SCROLL_TO_CONTROLS',panel.debug_show_bf120_trial_controls())
   state.update(phase='capture',at=time.monotonic());return
  if state['phase']=='capture' and time.monotonic()-state['at']>1:
   unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+png+'" -nosuffix',pc)
   state.update(phase='finish',at=time.monotonic());return
  if state['phase']=='finish' and time.monotonic()-state['at']>2:
   check('RENDER',os.path.isfile(png));finish()
 except Exception:report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
