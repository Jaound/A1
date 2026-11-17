"""보유 영웅의 실제 임무·성장·원수단 회수 API 검사. OS 입력 검증은 아니다."""
import json, os, time, traceback
import unreal

report={'passed':None,'kind':'보유 편제 PIE API','checks':[],'errors':[]}
state={'phase':0,'start':time.monotonic()}
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
def p(o,n): return o.get_editor_property(n)
def check(n,v,d=''):
 report['checks'].append({'id':n,'passed':bool(v),'detail':str(d)})
 unreal.log('OVOWN '+n+' '+str(bool(v))+' '+str(d))
 if not v: raise AssertionError(n+': '+str(d))
def gid(v):return tuple(p(v,x) for x in ('a','b','c','d'))
def restart_snapshot(pc):
 mission=p(pc,'mission');roster=p(pc,'heroes');interior=p(pc,'interior');transport=p(pc,'transport')
 return (p(mission,'battle_id'),p(mission,'uses_owned_roster'),p(mission,'game_time'),p(mission,'result'),tuple(p(mission,'attack_unit_ids')),
         tuple((gid(p(h,'hero_id')),p(h,'runtime_unit_id'),p(h,'runtime_person_id'),p(h,'xp'),p(h,'current_hp'),p(h,'status'),tuple(p(h,'skill_ranks'))) for h in roster.get_heroes(0)),
         tuple((p(u,'unit_id'),p(u,'person_id'),p(u,'hp'),p(u,'state'),p(u,'ship').get_path_name() if p(u,'ship') else '') for u in p(interior,'units')),
         tuple((p(person,'person_id'),p(person,'state'),p(person,'pod_id')) for person in p(transport,'persons')),
         tuple((p(pod,'pod_id'),p(pod,'person_id'),p(pod,'state'),p(pod,'hp')) for pod in p(transport,'pods')),
         tuple((s.get_path_name(),p(p(s,'systems'),'hull_hp'),p(p(s,'systems'),'shield')) for s in p(pc,'ships')))
def finish():
 report['passed']=bool(report['checks']) and all(x['passed'] for x in report['checks']) and not report['errors']
 dest=os.path.join(unreal.Paths.project_saved_dir(),'Validation','OwnedMission.json')
 with open(dest,'w',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
 unreal.unregister_slate_post_tick_callback(state['handle'])
def tick(dt):
 try:
  if state['phase']==0:
   level.editor_request_begin_play();state['phase']=1;return
  world=editor.get_game_world()
  if not world:return
  pc=unreal.GameplayStatics.get_player_controller(world,0)
  if not pc or not hasattr(pc,'step_simulation'):
   if time.monotonic()-state['start']>30:raise RuntimeError('30초 내 플레이어 컨트롤러를 찾지 못함')
   return
  if state['phase']==1:
   roster=p(pc,'heroes');dev=p(pc,'development');mission=p(pc,'mission')
   check('OWN-ROOM',dev.open_roster_room(),p(dev,'last_message'))
   ids=[]
   for role in [unreal.OVInteriorRole.MELEE,unreal.OVInteriorRole.RANGED,unreal.OVInteriorRole.SUPPORT,unreal.OVInteriorRole.COMMANDER]:
    h=roster.create_dev_hero(0,role,'검증 '+str(role));ids.append(h);check('OWN-RESERVE-'+str(role),roster.set_deployment(h,True))
   check('OWN-START',mission.start_roster_mission(pc,False,260112,False),p(mission,'status'))
   check('OWN-AIM-PROFILE',p(p(p(pc,'combat'),'settings'),'hero_d114_aim'))
   check('OWN-UNIQUE-4',len({gid(x) for x in ids})==4)
   check('OWN-INITIAL-REAL-UNITS',all(p(roster.get_hero(x),'runtime_unit_id')>0 for x in ids))
   before=restart_snapshot(pc)
   pc.action('RestartScene')
   check('OWN-RESTART-SCENE-PRESERVES-OPERATION',before==restart_snapshot(pc))
   check('OWN-RESTART-REASON',p(pc,'last_action')=='재시작 보류 · 회수 후 편성실에서 재출전하세요')
   pc.action('MissionRestart')
   check('OWN-RESTART-ALIAS-PRESERVES-OPERATION',before==restart_snapshot(pc))
   mission.set_auto_player(True)
   if p(pc,'simulation_paused'):pc.action('Pause')
   pc.action('Speed1')
   state.update(phase=2,ids=ids,roster=roster,mission=mission,pc=pc)
  elif state['phase']==2:
   pc=state['pc'];mission=state['mission']
   for _ in range(60):
    if p(mission,'result')!=unreal.OVMissionResult.ONGOING:break
    pc.step_simulation(.1)
   if p(mission,'result')==unreal.OVMissionResult.ONGOING and p(mission,'game_time')<600:return
   score=mission.get_score();roster=state['roster']
   check('OWN-REAL-SHIELD-DOWN',p(score,'shield_break_at')>=0)
   check('OWN-REAL-ENTRY',p(score,'entered')>0)
   check('OWN-REAL-OPERATION',p(score,'shield_operation_at')>=0 or p(score,'bridge_capture_at')>=0)
   check('OWN-ORIGINAL-TRANSPORT-RETURN',p(score,'returned_after_entry')>0)
   check('OWN-RESULT',p(mission,'result')==unreal.OVMissionResult.VICTORY,p(mission,'result'))
   survivors=[x for x in state['ids'] if p(roster.get_hero(x),'status')!=unreal.OVHeroStatus.DEAD]
   check('OWN-PARTICIPATION',all(mission.get_hero_participation_seconds(x)>0 for x in survivors))
   check('OWN-XP-REAL-RESULT',any(p(roster.get_hero(x),'level')>1 for x in survivors))
   snapshot=[(gid(x),p(roster.get_hero(x),'xp'),p(roster.get_hero(x),'level')) for x in state['ids']]
   pc.step_simulation(2)
   check('OWN-XP-ONCE',snapshot==[(gid(x),p(roster.get_hero(x),'xp'),p(roster.get_hero(x),'level')) for x in state['ids']])
   report['game_seconds']=p(mission,'game_time');report['returned']=p(score,'returned_after_entry')
   report['heroes']=[{'id':gid(x),'level':p(roster.get_hero(x),'level'),'xp':p(roster.get_hero(x),'xp'),'participation':mission.get_hero_participation_seconds(x)} for x in state['ids']]
   before=restart_snapshot(pc)
   pc.action('RestartScene')
   check('OWN-RESTART-RESULT-AND-LOSSES-PRESERVED',before==restart_snapshot(pc))
   if not p(pc,'simulation_paused'):pc.action('Pause')
   finish()
 except Exception:
  report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
