"""후보 FBX를 Blender에 재반입해 삼각형·UV와 입력 해시를 검증한다."""
import hashlib
import json
from pathlib import Path
import sys

import bpy

args = sys.argv[sys.argv.index('--') + 1:]
if len(args) != 2:
    raise ValueError('후보 보고서와 새 결과 경로가 필요합니다')
source, out = map(lambda arg: Path(arg).resolve(), args)
if not source.is_file() or out.exists():
    raise RuntimeError('후보 보고서가 없거나 결과가 이미 존재합니다')
data = json.loads(source.read_text(encoding='utf8'))

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

rows = []
for part in data['parts']:
    file = Path(part['fbx'])
    before = sha(file)
    if before != part['sha256']:
        raise RuntimeError('입력 해시가 다릅니다: ' + part['name'])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(file), use_image_search=False)
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if len(meshes) != 1:
        raise RuntimeError('단일 메시가 아닙니다: ' + part['name'])
    mesh = meshes[0].data
    mesh.calc_loop_triangles()
    count = len(mesh.loop_triangles)
    if count != part['triangles'] or not mesh.uv_layers or sha(file) != before:
        raise RuntimeError('재반입 검증 실패: ' + part['name'])
    rows.append({'name': part['name'], 'source_sha256': before,
                 'triangles': count, 'uv_layers': [layer.name for layer in mesh.uv_layers],
                 'material_slots': len(mesh.materials)})
if len(rows) != 2:
    raise RuntimeError('받침과 회전부 두 개가 필요합니다')
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({'scope': 'FBX 재반입, UV와 삼각형 수만 확인. 재질·가동·게임 적용 검증 아님.',
                           'passed': True, 'parts': rows}, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('OV_WEAPON_YAW_EXPORT_VALIDATED', out)
