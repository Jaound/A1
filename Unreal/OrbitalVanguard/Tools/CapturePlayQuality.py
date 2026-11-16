"""Engine-rendered UI evidence, never a claim of physical input validation.
Use OV_CAPTURE_LABEL=Before/After; runs only transient PIE scenes and exits.
"""
import unreal, os, time, json, traceback
label = os.environ.get('OV_CAPTURE_LABEL', 'Before')
out = os.path.join(unreal.Paths.project_saved_dir(), 'Validation', 'PlayQuality' + label)
os.makedirs(out, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state = {'phase': 0, 'at': time.monotonic(), 'index': 0}
cases = [(scene, dev, detail) for scene in ('F5', 'F6') for dev in (False, True) for detail in (False, True)]
def tick(dt):
    try:
        if state['phase'] == 0:
            level.editor_request_begin_play(); state['phase'] = 1; return
        world = editor.get_game_world()
        if not world: return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or len(pc.get_editor_property('ships')) < 3: return
        if state['phase'] == 1:
            if state['index'] == len(cases):
                unreal.unregister_slate_post_tick_callback(state['handle'])
                unreal.SystemLibrary.quit_editor(); return
            scene, dev, detail = cases[state['index']]
            if scene == 'F5': pc.start_combat_range()
            else: pc.start_calibration_range(1)
            pc.set_editor_property('developer_mode', dev)
            hud = pc.get_hud()
            hud.set_details_open(detail)
            state['phase'] = 2; state['at'] = time.monotonic(); return
        if time.monotonic() - state['at'] < 1: return
        if state['phase'] == 2:
            scene, dev, detail = cases[state['index']]
            name = os.path.abspath(os.path.join(out, f'{scene}_DEV{int(dev)}_DETAIL{int(detail)}.png')).replace('\\','/')
            unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename=' + name, pc)
            state['phase'] = 3; state['at'] = time.monotonic(); return
        state['index'] += 1; state['phase'] = 1
    except Exception:
        unreal.log_error(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(state['handle'])
state['handle'] = unreal.register_slate_post_tick_callback(tick)
