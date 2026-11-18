"""기존 통합 임무 검사를 보존하고 원본 무장 시험 활성 상태로 별도 저장한다."""
import os
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
for name in ('CIWSBTrial', 'LB30Trial', 'BF120SourceTrial', 'SHC400Trial', 'RG200Trial', 'PlasmaTrial'):
    unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'ov.Presentation.' + name + ' 1')
tool = os.path.join(unreal.Paths.project_dir(), 'Tools', 'ValidateIntegratedMission.py')
with open(tool, encoding='utf8') as file:
    source = file.read()
if source.count('"IntegratedMission.json"') != 1:
    raise RuntimeError('원래 보고서를 안전하게 분리할 수 없습니다')
source = source.replace('"IntegratedMission.json"', '"IntegratedMission_Source7.json"')
run = os.environ.get('OV_MISSION_SOURCE7_RUN', 'first')
if not run.replace('_', '').isalnum(): raise ValueError('잘못된 검사 ID')
output = 'IntegratedMission_Source7_' + run + '.json'
target = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Validation', output)
if os.path.exists(target): raise FileExistsError('기존 검사 보고서는 덮어쓰지 않습니다: ' + target)
source = source.replace('"IntegratedMission_Source7.json"', '"' + output + '"')
exec(compile(source, tool, 'exec'), globals())
