"""별도 후보의 원본 해시와 제한된 회전 간섭 결과를 보존한다.

간섭 검사는 절단 덮개를 제외한 표면 비교이며, 합격이나 실제 게임 적용 판정이 아니다.
"""
import hashlib
import json
from pathlib import Path

root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponYawCandidates_v1'
project = Path(__file__).resolve().parents[1]
out = project.parents[1] / 'docs/validation/PlayUIR2/LU_원본무장9종_분리간격검사.json'
if out.exists():
    raise FileExistsError(out)
rows = []
for trial in sorted(root.glob('*/report.json')):
    data = json.loads(trial.read_text(encoding='utf8'))
    ident = trial.parent.name
    geometry = root / (ident + '_intersections.json')
    if not geometry.is_file():
        raise FileNotFoundError(geometry)
    overlap = json.loads(geometry.read_text(encoding='utf8'))
    clearance_path = root / (ident + '_clearance001.json')
    if not clearance_path.is_file():
        raise FileNotFoundError(clearance_path)
    clearance = json.loads(clearance_path.read_text(encoding='utf8'))
    if data['source_before'] != data['source_after'] or overlap['source_before'] != overlap['source_after']:
        raise RuntimeError('원본 또는 후보 변경: ' + ident)
    if clearance['source_before'] != overlap['source_before'] or clearance['source_after'] != overlap['source_before']:
        raise RuntimeError('간격 검사 후보 해시 불일치: ' + ident)
    with Path(data['source']).open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != data['source_before']:
            raise RuntimeError('원본 해시 불일치: ' + ident)
    rows.append({'id': ident, 'source_sha256': data['source_before'],
                 'candidate_sha256': overlap['source_before'], 'cut_z': data['cut_z'],
                 'trial_pivot': data['trial_pivot'], 'parts': data['parts'],
                 'angles': overlap['angles'], 'trial_clearance': clearance['trial_clearance'],
                 'clearance_angles': clearance['angles']})
if len(rows) != 9:
    raise RuntimeError('9종 후보 및 간섭 검사 필요')
out.write_text(json.dumps({'scope': '비파괴 시험 후보. 간격 0과 0.001 단위에서 덮개 제외 수평 회전 5개 각도의 표면 교차만 측정; '
                           '피치·포구·재질·선체 장착·성능·전장 적용 미승인.',
                           'models': rows}, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('OV_WEAPON_YAW_CANDIDATES', out)
