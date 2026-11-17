"""반복 생성·폐기와 UMG 조회가 실제 재고/HP/명단을 변경하지 않는지 확인한다."""
import json, os, time, traceback
import unreal
report={'passed':None,'kind':'반복 수명 PIE API (직접 입력 아님)','checks':[],'errors':[]}
state={'phase':0,'start':time.monotonic()}
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
def p(o,k):return o.get_editor_property(k)
def check(n,v,d=''):
 report['checks'].append({'id':n,'passed':bool(v),'detail':str(d)})
 if not v:raise AssertionError(n+': '+str(d))
def finish():
 report['passed']=bool(report['checks']) and all(x['passed'] for x in report['checks']) and not report['errors']
 with open(os.path.join(unreal.Paths.project_saved_dir(),'Validation','DevelopmentRepeat.json'),'w',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
 unreal.unregister_slate_post_tick_callback(state['handle'])
def tick(dt):
 try:
  if state['phase']==0:level.editor_request_begin_play();state['phase']=1;return
  world=editor.get_game_world()
  pc=unreal.GameplayStatics.get_player_controller(world,0) if world else None
  if not pc:return
  check('REP-RANGE',pc.start_phase2c_range(2))
  pc.set_editor_property('developer_mode',True)
  dev=p(pc,'development');combat=p(pc,'combat');wing=p(pc,'carrier');transit=p(pc,'transport');interior=p(pc,'interior')
  ships=len(p(pc,'ships'));combat_count=len(p(combat,'ships'));aircraft=[(p(c,'aircraft_id'),p(c,'hp')) for c in p(wing,'aircraft')]
  persons=len(p(transit,'persons'));units=len(p(interior,'units'));hero_count=len(p(p(pc,'heroes'),'heroes'))
  for i in range(60):
   spawned=dev.spawn_ships('corvette',0,1,unreal.Vector(125000,120000,2000),30000,0,0,False,False)
   if len(spawned)!=1:check('REP-SPAWN-DELETE',False,str(i)+p(dev,'last_message'))
   if not dev.delete_ship(spawned[0]):check('REP-SPAWN-DELETE',False,str(i)+p(dev,'last_message'))
  check('REP-60-SPAWN-DELETE',True)
  check('REP-ACTOR-REGISTRATIONS',len(p(pc,'ships'))==ships and len(p(combat,'ships'))==combat_count)
  hud=pc.get_hud();widget=hud.get_development_panel();identity=widget.get_path_name()
  hp=[p(p(s,'systems'),'hull_hp') for s in p(pc,'ships')]
  for i in range(50):
   for page in [1,0,2,3]:
    if not widget.open_page(page):check('REP-PAGES',False,str((i,page)))
   pc.action('Developer');pc.action('Developer')
  check('REP-WIDGET-IDENTITY',hud.get_development_panel().get_path_name()==identity)
  check('REP-50-TAB-TOGGLE-CYCLES',True)
  check('REP-HP-UNCHANGED',hp==[p(p(s,'systems'),'hull_hp') for s in p(pc,'ships')])
  check('REP-AIRCRAFT-ID-HP',aircraft==[(p(c,'aircraft_id'),p(c,'hp')) for c in p(wing,'aircraft')])
  check('REP-PEOPLE-UNCHANGED',persons==len(p(transit,'persons')) and units==len(p(interior,'units')) and hero_count==len(p(p(pc,'heroes'),'heroes')))
  check('REP-DEV-LIST-CLEAN',len(p(dev,'spawned_ships'))==0)
  # F7 records must not return when F6 spawning re-enables the subsystems.
  for i in range(6):
   pc.start_phase2c_range(5);pc.set_editor_property('developer_mode',True)
   check('REP-OLD-PASSENGERS-'+str(i),len(p(transit,'persons'))>0)
   pc.start_calibration_range(1);pc.set_editor_property('developer_mode',True)
   check('REP-SCENE-RECORDS-CLEAR-'+str(i),len(p(transit,'persons'))==0 and len(p(transit,'pods'))==0 and len(p(interior,'units'))==0)
   spawned=dev.spawn_ships('repair',0,1,unreal.Vector(125000,120000,2000),30000,0,0,False,False)
   check('REP-REACTIVATE-NO-GHOST-'+str(i),len(spawned)==1 and len(p(transit,'persons'))==0 and len(p(interior,'units'))==0)
   check('REP-POST-RESET-DELETE-'+str(i),dev.delete_ship(spawned[0]))
  report['repetitions']={'spawn_delete':60,'tabs_and_toggle':50};finish()
 except Exception:report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
