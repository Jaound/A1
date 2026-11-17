"""기존 OBB와 PC14 시험 충돌 후보의 동일 F6 조건 비교. 밸런스 확정이 아니다."""
import json, os, time, traceback
import unreal
saved=unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
out=os.path.join(saved,'Validation','PC14CollisionComparison.json');os.makedirs(os.path.dirname(out),exist_ok=True)
report={'passed':None,'physical_input':False,'seed':260112,'game_seconds':30,'cases':[],'checks':[],'errors':[],
        'scope':'동일 F6 시험 편성·시드의 30초 API 비교. 실제 렌더 성능이나 전체 밸런스 판정 아님.'}
state={'phase':'start','started':time.monotonic()}
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def p(o,k):return o.get_editor_property(k)
def check(key,ok,detail=''):
 report['checks'].append({'id':key,'passed':bool(ok),'detail':str(detail)})
 if not ok:raise AssertionError(key+': '+str(detail))
def run_case(pc,trial):
 check(('TRIAL' if trial else 'BASE')+'_SCENE',pc.start_calibration_range(1))
 ships=list(p(pc,'ships'));vector,corvette,enemy=ships;combat=p(pc,'combat');layout=p(p(corvette,'systems'),'layout')
 for ship in ships:ship.stop_movement();ship.set_automation(False,True)
 check(('TRIAL' if trial else 'BASE')+'_TARGET',combat.set_target(enemy,corvette))
 check(('TRIAL' if trial else 'BASE')+'_TARGET_FRIEND',combat.set_target(vector,enemy))
 check(('TRIAL' if trial else 'BASE')+'_TARGET_CORVETTE',combat.set_target(corvette,enemy))
 if trial:
  path=os.path.normpath(os.path.join(unreal.Paths.project_dir(),'..','..','docs','validation','PlayUIR2','HA_PC14충돌후보.json'))
  with open(path,encoding='utf-8') as f:data=json.load(f)
  boxes=[]
  for entry in data['boxes']:
   box=unreal.OVCombatHullBox();box.set_editor_property('local_center',unreal.Vector(*entry['local_center_cm']))
   box.set_editor_property('half_extent',unreal.Vector(*entry['half_extent_cm']));boxes.append(box)
  layout.set_editor_property('combat_hull_boxes',boxes)
  mounts=list(p(layout,'weapon_mounts'));m=mounts[0];position=p(m,'local_position');position.z=3.6062369788711637
  m.set_editor_property('local_position',position);mounts[0]=m;layout.set_editor_property('weapon_mounts',mounts)
  check('TRIAL_BOX_COUNT',len(p(layout,'combat_hull_boxes'))==27)
 else:check('BASE_ORIGINAL_BOX',len(p(layout,'combat_hull_boxes'))==0)
 if p(pc,'simulation_paused'):pc.action('Pause')
 events={};start=time.perf_counter()
 for _ in range(120):
  pc.step_simulation(.25)
  for event in p(combat,'recent_events'): events[p(event,'attack_id')]=event
 elapsed=time.perf_counter()-start
 pc.action('Pause')
 records=list(events.values());hits=[e for e in records if p(e,'result')==unreal.OVAttackResult.HIT]
 return {'case':'측정 상자와 표면 장착점' if trial else '기존 OBB와 기존 장착점','game_time':p(combat,'game_time'),
         'sim_wall_seconds':elapsed,'shots':p(combat,'shot_count'),'hits':p(combat,'hit_count'),
         'mount_hits':sum(str(p(e,'impact_weapon_id'))!='None' for e in hits),
         'hulls':[round(p(p(s,'systems'),'hull_hp'),3) for s in ships],
         'shields':[round(p(p(s,'systems'),'shield'),3) for s in ships],
         'wrecked':[p(p(s,'systems'),'wrecked') for s in ships],
         'layout_box_count':len(p(layout,'combat_hull_boxes'))}
def finish():
 report['passed']=bool(report['checks']) and all(c['passed'] for c in report['checks']) and not report['errors']
 with open(out,'w',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
 unreal.unregister_slate_post_tick_callback(state['handle'])
def tick(_):
 try:
  if time.monotonic()-state['started']>120:raise RuntimeError('PC14 충돌 비교 시간 초과')
  if state['phase']=='start':level.editor_request_begin_play();state['phase']='wait';return
  world=editor.get_game_world();pc=unreal.GameplayStatics.get_player_controller(world,0) if world else None
  if not pc or len(p(pc,'ships'))<3:return
  if state['phase']=='wait':
   report['cases']=[run_case(pc,False),run_case(pc,True)]
   a,b=report['cases'];check('SAME_GAME_TIME',abs(a['game_time']-b['game_time'])<.01,(a['game_time'],b['game_time']))
   check('ACTUAL_COMBAT',a['shots']>0 and b['shots']>0 and a['hits']>0 and b['hits']>0)
   finish()
 except Exception:
  report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
