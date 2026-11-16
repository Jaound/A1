"""API-driven game-time clip and rendered scene evidence; not manual play evidence."""
import os, time, unreal, traceback
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out=os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(),'Validation','Phase2CClip'))
os.makedirs(out,exist_ok=True)
state={'phase':0,'index':0,'at':time.monotonic()}
def tick(dt):
 try:
  if state['phase']==0:level.editor_request_begin_play();state['phase']=1;return
  world=editor.get_game_world()
  if not world:return
  pc=unreal.GameplayStatics.get_player_controller(world,0)
  if not pc or len(pc.get_editor_property('ships'))<3:return
  if state['phase']==1:
   (getattr(pc,"start_phase2c_range",None) or getattr(pc,"start_phase2_c_range"))(5);pc.set_editor_property('developer_mode',False)
   pc.get_hud().set_details_open(False)
   for ship in pc.get_editor_property('ships'):ship.set_automation(False,True)
   pc.action('LaunchAircraft');pc.action('LaunchPods')
   state.update(phase=2,at=time.monotonic());return
  if time.monotonic()-state['at']<.12:return
  if state['index']>=72:
   unreal.unregister_slate_post_tick_callback(state['handle']);unreal.SystemLibrary.quit_editor();return
  pc.action('Pause');pc.step_simulation(1/12);pc.action('Pause')
  path=os.path.join(out,f'frame{state["index"]:04}.png').replace('\\','/')
  unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename='+path+' -nosuffix',pc)
  state['index']+=1;state['at']=time.monotonic()
 except Exception:
  unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(state['handle']);unreal.SystemLibrary.quit_editor()
state['handle']=unreal.register_slate_post_tick_callback(tick)
