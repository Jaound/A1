"""이동 목표 대함 기동의 UE 렌더 증거. OS 입력이나 조작감 검증이 아니다."""
import unreal, os, time, traceback
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state = {'phase': 0, 'at': time.monotonic()}
out = os.path.join(unreal.Paths.project_saved_dir(), 'Validation', 'StrikeScreens')
os.makedirs(out, exist_ok=True)

def tick(dt):
    try:
        if state['phase'] == 0:
            level.editor_request_begin_play(); state['phase'] = 1; return
        world = editor.get_game_world()
        if not world: return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or len(pc.get_editor_property('ships')) < 3: return
        if state['phase'] == 1:
            script = os.path.join(unreal.Paths.project_dir(), 'Tools', 'MeasureAircraftManeuvers.py')
            # 동일 측정 fixture 함수만 사용한다. 측정기의 Slate callback은 등록하지 않는다.
            namespace = {}
            with open(script, encoding='utf-8') as source:
                exec(compile(source.read().split('def tick(dt):')[0], script, 'exec'), namespace)
            namespace['run'](pc, 13, True, 1)
            ships = pc.get_editor_property('ships')
            pc.set_editor_property('camera_focus', ships[2].get_actor_location())
            pc.set_editor_property('camera_distance', 48000)
            pc.set_editor_property('developer_mode', False)
            pc.get_hud().set_details_open(False)
            state.update(phase=2, at=time.monotonic()); return
        if time.monotonic()-state['at'] < .5: return
        if state['phase'] in (2, 3):
            name = '대함기동_기본.png' if state['phase'] == 2 else '대함기동_DEV.png'
            path = os.path.abspath(os.path.join(out, name)).replace('\\', '/')
            unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename='+path+' -nosuffix', pc)
            state.update(phase=state['phase']+1, at=time.monotonic()); return
        if state['phase'] == 4:
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.SystemLibrary.quit_editor()
    except Exception:
        unreal.log_error(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.SystemLibrary.quit_editor()

# 기본 캡처가 완료된 다음 실제 프레임에서 DEV 상태를 변경한다.
def staged_tick(dt):
    if state['phase'] == 3:
        world = editor.get_game_world()
        if world:
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if pc: pc.set_editor_property('developer_mode', True)
    tick(dt)
state['handle'] = unreal.register_slate_post_tick_callback(staged_tick)
