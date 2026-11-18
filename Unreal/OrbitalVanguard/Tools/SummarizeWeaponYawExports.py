"""9종 후보의 FBX 재반입 결과를 원본 해시와 함께 보존한다."""
import hashlib
import json
from pathlib import Path

root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponYawExports_v1'
project = Path(__file__).resolve().parents[1]
out = project.parents[1] / 'docs/validation/PlayUIR2/LV_원본무장9종_분리FBX검사.json'
if out.exists():
    raise FileExistsError(out)
rows = []
for candidate in sorted(root.glob('*/report.json')):
    export = json.loads(candidate.read_text(encoding='utf8'))
    verify_path = candidate.parent / 'reimport.json'
    if not verify_path.is_file():
        raise FileNotFoundError(verify_path)
    verified = json.loads(verify_path.read_text(encoding='utf8'))
    if not verified['passed'] or export['source_before'] != export['source_after']:
        raise RuntimeError('후보 또는 FBX 검증 실패: ' + candidate.parent.name)
    for part in export['parts']:
        with Path(part['fbx']).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != part['sha256']:
                raise RuntimeError('FBX 해시가 바뀌었습니다: ' + part['name'])
    rows.append({'id': candidate.parent.name, 'source_sha256': export['source_before'],
                 'parts': export['parts'], 'reimport': verified['parts']})
if len(rows) != 9:
    raise RuntimeError('9종 모두 필요합니다')
out.write_text(json.dumps({'scope': '분리 후보 FBX의 해시·삼각형·UV 확인; 전장 적용 검증 아님.',
                           'models': rows}, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('OV_WEAPON_YAW_EXPORTS', out)
