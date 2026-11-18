"""격리된 원본 기반 부품의 UE 로컬 경계를 측정한다. 포구를 자동 확정하지 않는다."""
import json
import os
from pathlib import Path
import traceback

import unreal

ident = os.environ.get('OV_WEAPON_PITCH_ID', '')
run = os.environ.get('OV_WEAPON_PITCH_RUN', '')
if not ident or not run or not ident.replace('_', '').isalnum() or not run.replace('_', '').isalnum():
    raise ValueError('후보 ID와 실행 ID가 필요합니다')
out = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation' / ('WeaponPitchBounds_' + ident + '_' + run + '.json')
if out.exists():
    raise FileExistsError(out)
report = {'passed': False, 'scope': '격리된 UE 메시 로컬 경계만 측정; 실제 포구·가동·장착 확정 아님.', 'parts': [], 'errors': []}
try:
    for folder, name in [('WeaponsYawTrial', 'Base'), ('WeaponsPitchTrial', 'Body'),
                         ('WeaponsPitchTrial', 'Barrel'), ('WeaponsPitchTrial', 'TrialNeck')]:
        path = '/Game/Presentation/' + folder + '/' + ident + '/SM_' + name
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError('메시 없음: ' + path)
        box = mesh.get_bounding_box()
        minimum, maximum = box.get_editor_property('min'), box.get_editor_property('max')
        report['parts'].append({'asset': path, 'min_cm': [minimum.x, minimum.y, minimum.z],
                                'max_cm': [maximum.x, maximum.y, maximum.z],
                                'triangles': mesh.get_num_triangles(0)})
    report['passed'] = len(report['parts']) == 4
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    unreal.SystemLibrary.quit_editor()
