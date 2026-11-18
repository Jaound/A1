"""격리된 회전 후보를 UE 시험용 FBX로 내보낸다. 게임 적용은 하지 않는다.

Blender --background <후보.blend> --python ExportWeaponYawCandidate.py -- <id> <새 출력 폴더>
기존 .blend와 원본은 저장하거나 덮어쓰지 않는다.
"""
import hashlib
import json
from pathlib import Path
import sys

import bpy

args = sys.argv[sys.argv.index('--') + 1:]
if len(args) != 2:
    raise ValueError('후보 ID와 새 출력 폴더가 필요합니다')
ident, out_arg = args
if not ident.replace('_', '').isalnum():
    raise ValueError('잘못된 후보 ID')
source = Path(bpy.data.filepath).resolve()
out = Path(out_arg).resolve()
if not source.is_file() or out.exists():
    raise RuntimeError('후보 파일이 없거나 출력 폴더가 이미 있습니다')

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

before = sha(source)
parts = []
for suffix in ('BaseTrial', 'YawTrial'):
    obj = bpy.data.objects.get(ident + '_' + suffix)
    if obj is None or obj.type != 'MESH':
        raise RuntimeError('후보 메시가 없습니다: ' + suffix)
    if not obj.data.uv_layers:
        raise RuntimeError('원본 UV가 없습니다: ' + suffix)
    parts.append(obj)
out.mkdir(parents=True)
rows = []
for obj in parts:
    origin = list(obj.location)
    obj.location = (0, 0, 0)
    obj.rotation_euler = (0, 0, 0)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.context.view_layer.update()
    target = out / (obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(target), use_selection=True, object_types={'MESH'},
                             add_leaf_bones=False, path_mode='AUTO', bake_anim=False)
    obj.data.calc_loop_triangles()
    rows.append({'name': obj.name, 'fbx': str(target), 'sha256': sha(target),
                 'source_origin': origin, 'triangles': len(obj.data.loop_triangles),
                 'uv_layers': [uv.name for uv in obj.data.uv_layers],
                 'materials': [mat.name if mat else None for mat in obj.data.materials]})
    obj.location = origin
after = sha(source)
if before != after:
    raise RuntimeError('입력 후보가 변경되었습니다')
report = {'scope': '격리된 상하 분리 후보 FBX. 포구·피치·재질·선체 장착·성능 미승인.',
          'source': str(source), 'source_before': before, 'source_after': after, 'parts': rows}
(out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('OV_WEAPON_YAW_EXPORT', out)
