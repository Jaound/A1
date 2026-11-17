"""통합 임무 항공대 버튼 회귀. PIE API이며 실제 마우스 입력이 아니다."""
import json,os,time,traceback,unreal
out=os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),'Validation','MissionAircraftButtons.json')
report={'passed':None,'checks':[],'errors':[],'physical_input':False}
state={'phase':0,'start':time.monotonic()}; editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def p(o,k):return o.get_editor_property(k)
def check(k,v,detail=''):
 report['checks'].append({'id':k,'passed':bool(v),'detail':str(detail)})
 if not v:raise AssertionError(k+': '+str(detail))
def records(wing,ship):
 return {p(a,'aircraft_id'):(p(a,'hp'),p(a,'shield')) for a in p(wing,'aircraft') if p(a,'carrier')==ship}
def finish():
 report['passed']=not report['errors'] and bool(report['checks']);open(out,'w',encoding='utf8').write(json.dumps(report,ensure_ascii=False,indent=2));unreal.unregister_slate_post_tick_callback(state['handle'])
def tick(dt):
 try:
  if time.monotonic()-state['start']>90:raise RuntimeError('시간 초과')
  if state['phase']==0:level.editor_request_begin_play();state['phase']=1;return
  world=editor.get_game_world();pc=unreal.GameplayStatics.get_player_controller(world,0) if world else None
  if not pc or len(p(pc,'ships'))<3:return
  if state['phase']==1:
   pc.action('MissionAttack');wing=p(pc,'carrier');ship=next(p(c,'carrier') for c in p(wing,'carriers') if p(c,'carrier') and p(p(p(c,'carrier'),'systems'),'faction_id')==0)
   pc.select_ship(ship);state.update(phase=2,ship=ship,before=records(wing,ship));return
  wing=p(pc,'carrier');ship=state['ship'];hud=pc.get_hud()
  if state['phase']==2:
   before=wing.get_aircraft_overview(ship);check('FIXTURE-AIRBORNE',p(before,'sortie')>0)
   hud.execute_ui_action('HangarRecallAircraft');after=wing.get_aircraft_overview(ship)
   check('MISSION-HANGAR-RECALL-ACTUAL',p(after,'returning')==p(before,'sortie') and p(after,'sortie')==0,(p(before,'sortie'),p(after,'returning')))
   check('RECALL-IDS-HP-PRESERVED',records(wing,ship)==state['before'])
   wing.simulate(30);docked=wing.get_aircraft_overview(ship)
   check('REAL-DOCKING',p(docked,'stored')>0,p(docked,'stored'))
   before_count=p(docked,'stored');before_relaunch=records(wing,ship);hud.execute_ui_action('HangarLaunchAircraft');launched=wing.get_aircraft_overview(ship)
   check('MISSION-HANGAR-LAUNCH-ACTUAL',p(launched,'sortie')>=before_count and p(launched,'stored')==0,(before_count,p(launched,'sortie'),p(launched,'stored')))
   check('RELAUNCH-IDS-HP-PRESERVED',records(wing,ship)==before_relaunch)
   hud.execute_ui_action('LaunchAircraft');check('MISSION-EMPTY-LAUNCH-FEEDBACK','출격 0대' in p(pc,'last_action') and records(wing,ship)==before_relaunch)
   hud.execute_ui_action('RecallAircraft');check('MISSION-COMMAND-CARD-RECALL',p(wing.get_aircraft_overview(ship),'returning')>0)
   wing.simulate(30);stored=p(wing.get_aircraft_overview(ship),'stored');before_card_launch=records(wing,ship)
   check('MISSION-COMMAND-CARD-DOCKING',stored>0)
   hud.execute_ui_action('LaunchAircraft');check('MISSION-COMMAND-CARD-LAUNCH',p(wing.get_aircraft_overview(ship),'sortie')>=stored and records(wing,ship)==before_card_launch)
   enemy=next(p(c,'carrier') for c in p(wing,'carriers') if p(c,'carrier') and p(p(p(c,'carrier'),'systems'),'faction_id')!=0)
   enemy_before=records(wing,enemy);enemy_state=wing.get_aircraft_overview(enemy);pc.select_ship(enemy)
   hud.execute_ui_action('RecallAircraft');enemy_after=wing.get_aircraft_overview(enemy)
   check('ENEMY-CARRIER-COMMAND-BLOCKED',records(wing,enemy)==enemy_before and p(enemy_after,'sortie')==p(enemy_state,'sortie') and p(enemy_after,'returning')==p(enemy_state,'returning'))
   pc.select_ship(ship);hud.execute_ui_action('WindowPin');hud.execute_ui_action('PinnedHangar_0')
   pc.select_ship(enemy);hud.execute_ui_action('WindowPin');hud.execute_ui_action('PinnedHangar_1')
   source_before=records(wing,ship);hud.execute_ui_action('PinnedRecall_1')
   check('PIN-ENEMY-COMMAND-BLOCKED',records(wing,enemy)==enemy_before and p(pc,'selected')==enemy)
   hud.execute_ui_action('PinnedRecall_0')
   check('PIN-RECALL-OWN-CARRIER-ONLY',p(wing.get_aircraft_overview(ship),'returning')>0 and p(pc,'selected')==enemy and records(wing,ship)==source_before)
   wing.simulate(30);stored=p(wing.get_aircraft_overview(ship),'stored');source_before=records(wing,ship)
   check('PIN-REAL-DOCKING',stored>0)
   hud.execute_ui_action('PinnedLaunch_0')
   check('PIN-LAUNCH-OWN-CARRIER-ONLY',p(wing.get_aircraft_overview(ship),'sortie')>=stored and p(pc,'selected')==enemy and records(wing,ship)==source_before)
   finish()
 except Exception:
  report['errors'].append(traceback.format_exc());finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
