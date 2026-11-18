"""원본 기반 CIWS-B 시험 스위치를 켠 2C PIE API와 렌더를 검증한다.

기존 2C 검사 ID를 그대로 실행한다. OS 마우스·키보드 입력 검사가 아니다.
"""
import os
import time
import traceback
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'ov.Presentation.CIWSBTrial 1')
lb30 = os.environ.get('OV_LB30_TRIAL', '0') == '1'
if lb30:
    unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'ov.Presentation.LB30Trial 1')
tool = os.path.join(unreal.Paths.project_dir(), 'Tools', 'ValidatePhase2C.py')
with open(tool, encoding='utf8') as file:
    source = file.read()
if source.count('"Phase2C.json"') != 1:
    raise RuntimeError('기존 2C 보고서 경로를 안전하게 분리할 수 없습니다')
run = os.environ.get('OV_CIWSB_RUN', 'first')
if not run.replace('_', '').isalnum():
    raise ValueError('잘못된 검사 ID')
source = source.replace('"Phase2C.json"', '"CIWSB_2C_' + run + '.json"')
exec(compile(source, tool, 'exec'), globals())
trial = {'phase': 'wait', 'started': time.monotonic()}

def finish_trial(dt):
    try:
        if trial['phase'] == 'wait':
            if 'passed' not in report:
                if time.monotonic() - trial['started'] > 360:
                    raise RuntimeError('2C 검사 대기 시간 초과')
                return
            if not report['passed']:
                raise RuntimeError('시험 스위치를 켠 2C 검사 실패')
            pc = state['pc']
            # Inspect the small calibration hull; the first mixed-scene carrier can surround a close camera.
            if not pc.start_calibration_range(1):
                raise RuntimeError('시각 확인용 F6 장면 생성 실패')
            candidates = []
            kinetic = []
            beams = []
            for ship in p(pc, 'ships'):
                ship.refresh_weapon_visuals()
                for component in ship.get_components_by_class(unreal.StaticMeshComponent):
                    mesh = component.get_editor_property('static_mesh')
                    if mesh and '/WeaponsPitchTrial/ciws_beam_b/SM_Barrel' in mesh.get_path_name():
                        candidates.append((ship, component))
                    if mesh and '/WeaponsPitchTrial/ciws_gatling_6/SM_Barrel' in mesh.get_path_name():
                        kinetic.append((ship, component))
                    if mesh and '/WeaponsPitchTrial/laser_turret_lb30/SM_Barrel' in mesh.get_path_name():
                        beams.append((ship, component))
            check('CIWSB-SOURCE-BARREL-LIVE', bool(candidates), len(candidates))
            check('CIWS6-SOURCE-BARREL-LIVE', bool(kinetic), len(kinetic))
            if lb30:
                check('LB30-SOURCE-BARREL-LIVE', bool(beams), len(beams))
            ship, barrel = candidates[0]
            pc.select_ship(ship)
            pc.set_editor_property('camera_focus', ship.get_actor_location())
            pc.set_editor_property('camera_distance', 4000)
            pc.set_editor_property('camera_yaw', -45)
            pc.set_editor_property('camera_pitch', -30)
            pc.update_camera()
            trial.update(phase='warmup', at=time.monotonic(), ship=ship)
            return
        if trial['phase'] == 'warmup' and time.monotonic() - trial['at'] > 1:
            png = os.path.join(root, 'Validation', 'CIWSB_Trial_Render_' + run + '.png').replace('\\', '/')
            if os.path.exists(png):
                raise FileExistsError(png)
            unreal.SystemLibrary.execute_console_command(state['world'], 'Shot SHOWUI filename="' + png + '" -nosuffix', state['pc'])
            trial.update(phase='capture', at=time.monotonic(), png=png)
            return
        if trial['phase'] == 'capture' and time.monotonic() - trial['at'] > 2:
            check('CIWSB-RENDER-CAPTURED', os.path.isfile(trial['png']), trial['png'])
            report['scope'] += '; 빔/실탄 원본 기반 포신 및 시험 연결부 표시, 물리 입력 아님'
            report['capture'] = trial['png']
            save()
            unreal.unregister_slate_post_tick_callback(trial['handle'])
            unreal.SystemLibrary.quit_editor()
    except Exception:
        report['errors'].append(traceback.format_exc())
        save()
        unreal.log_error(report['errors'][-1])
        unreal.unregister_slate_post_tick_callback(trial['handle'])
        unreal.SystemLibrary.quit_editor()

trial['handle'] = unreal.register_slate_post_tick_callback(finish_trial)
