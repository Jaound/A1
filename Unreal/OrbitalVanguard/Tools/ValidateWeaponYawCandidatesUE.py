"""새 에디터에서 격리 반입된 후보의 메시·원본 재질·LOD를 다시 읽는다."""
import json
import os
from pathlib import Path
import traceback

import unreal

ids = [s for s in os.environ.get('OV_WEAPON_YAW_IDS', '').split(',') if s]
run = os.environ.get('OV_WEAPON_YAW_VALIDATE_RUN', 'first')
if not ids or any(not i.replace('_', '').isalnum() for i in ids) or not run.replace('_', '').isalnum():
    raise ValueError('확인할 후보 ID와 검사 ID가 필요합니다')
out = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation' / ('WeaponYawReload_' + run + '.json')
if out.exists():
    raise FileExistsError(out)
report = {'passed': False, 'scope': '격리 자산 재로드·LOD·재질 참조 검사; 전장 적용 및 렌더 검증 아님.',
          'models': [], 'errors': []}
try:
    editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for ident in ids:
        parts = []
        expected_material = '/Game/Presentation/WeaponsFull/' + ident + '/M_Source.M_Source'
        for label in ('Base', 'Yaw'):
            path = '/Game/Presentation/WeaponsYawTrial/' + ident + '/SM_' + label
            mesh = unreal.EditorAssetLibrary.load_asset(path)
            if not isinstance(mesh, unreal.StaticMesh):
                raise RuntimeError('메시 누락: ' + path)
            counts = [mesh.get_num_triangles(i) for i in range(editor.get_lod_count(mesh))]
            material = mesh.get_material(0)
            if len(counts) != 3 or not 0 < counts[2] < counts[1] < counts[0]:
                raise RuntimeError('LOD 불일치: ' + path)
            if not material or material.get_path_name() != expected_material:
                raise RuntimeError('원본 재질 참조 누락: ' + path)
            if editor.get_num_uv_channels(mesh, 0) < 1 or editor.get_simple_collision_count(mesh) != 0:
                raise RuntimeError('UV 또는 표시 충돌 불일치: ' + path)
            parts.append({'asset': path, 'triangles': counts, 'material': material.get_path_name()})
        report['models'].append({'id': ident, 'parts': parts})
    report['passed'] = len(report['models']) == len(ids)
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    unreal.SystemLibrary.quit_editor()
