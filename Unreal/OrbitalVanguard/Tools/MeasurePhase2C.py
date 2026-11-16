"""Rendered Editor load measurements. Not a packaged-game performance guarantee."""
import unreal, time, os, json, statistics, traceback
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
output=os.path.join(unreal.Paths.project_saved_dir(),'Validation','Phase2CPerformance.json')
state={'phase':0,'case':0,'at':time.monotonic()}
report={'kind':'real Editor/Slate frames, not API batch timings','conditions':{'seed':260112,'rhi':'DX11','speed':1,'vsync':'engine default','frame_cap':'engine default','warmup_wall_s':3,'measurement_wall_s':8},'samples':[],'errors':[]}
def p(o,k):return o.get_editor_property(k)
def tick(dt):
 try:
  if state['phase']==0:level.editor_request_begin_play();state['phase']=1;return
  world=editor.get_game_world()
  if not world:return
  pc=unreal.GameplayStatics.get_player_controller(world,0)
  if not pc or len(p(pc,'ships'))<3:return
  if state['phase']==1:
   if state['case']==3:
    (getattr(pc,"start_phase2c_range",None) or getattr(pc,"start_phase2_c_range"))(5)
    with open(output,'w',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
    unreal.unregister_slate_post_tick_callback(state['handle']);unreal.SystemLibrary.quit_editor();return
   (getattr(pc,"start_phase2c_stress",None) or getattr(pc,"start_phase2_c_stress"))((0,1,3)[state['case']])
   pc.set_editor_property('developer_mode',False);pc.get_hud().set_details_open(False)
   for ship in p(pc,'ships'):ship.set_automation(False,True)
   pc.action('LaunchAircraft');pc.action('Pause')
   unreal.SystemLibrary.execute_console_command(world,'csvprofile start',pc)
   state.update(phase=2,at=time.monotonic(),frames=[],game_start=p(p(pc,'combat'),'game_time'));return
  if state['phase']==2:
   if time.monotonic()-state['at']<3:return
   state.update(phase=3,at=time.monotonic(),game_start=p(p(pc,'combat'),'game_time'));return
  state['frames'].append(dt*1000)
  if time.monotonic()-state['at']<8:return
  pc.action('Pause');unreal.SystemLibrary.execute_console_command(world,'csvprofile stop',pc)
  a=sorted(state['frames']);wing=p(pc,'carrier');combat=p(pc,'combat')
  report['samples'].append({'ships':len(p(pc,'ships')),'aircraft_records':len(p(wing,'aircraft')),'live_aircraft':sum(str(p(c,'state')) not in ('OVAircraftState.STORED','OVAircraftState.WRECKED') for c in p(wing,'aircraft')),'weapon_attacks':len(p(combat,'attacks')),'fighter_bolts':len(p(wing,'bolts')),'viewport':str(pc.get_viewport_size()),'wall_seconds':time.monotonic()-state['at'],'game_seconds':p(combat,'game_time')-state['game_start'],'frames':len(a),'mean_frame_ms':statistics.mean(a),'p95_frame_ms':a[min(len(a)-1,int(.95*len(a)))],'max_frame_ms':max(a)})
  state['case']+=1;state['phase']=1
 except Exception:
  report['errors'].append(traceback.format_exc())
  with open(output,'w',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
  unreal.unregister_slate_post_tick_callback(state['handle']);unreal.SystemLibrary.quit_editor()
state['handle']=unreal.register_slate_post_tick_callback(tick)
