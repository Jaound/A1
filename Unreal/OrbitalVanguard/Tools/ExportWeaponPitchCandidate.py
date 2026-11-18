"""격리된 포신 후보의 몸체·포신·시험 연결부를 각각 내보낸다."""
import hashlib
import json
from pathlib import Path
import sys

import bpy
from mathutils import Matrix

args = sys.argv[sys.argv.index('--') + 1:]
if len(args) != 2:
    raise ValueError('후보 ID와 새 출력 폴더가 필요합니다')
ident, out_arg = args
if not ident.replace('_', '').isalnum():
    raise ValueError('잘못된 후보 ID')
source = Path(bpy.data.filepath).resolve()
out = Path(out_arg).resolve()
if not source.is_file() or out.exists():
    raise RuntimeError('후보 파일이 없거나 출력 폴더가 이미 존재합니다')

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

before = sha(source)
parts = []
for suffix in ('YawBody', 'PitchTrial', 'TrialPitchNeck'):
    obj = bpy.data.objects.get(ident + '_' + suffix)
    if obj is None or obj.type != 'MESH' or not obj.data.uv_layers:
        raise RuntimeError('후보 메시 또는 UV가 없습니다: ' + suffix)
    parts.append(obj)
out.mkdir(parents=True)
rows = []
for obj in parts:
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    mesh = obj.data.copy()
    mesh.transform(transform.to_3x3().to_4x4())
    export_obj = bpy.data.objects.new(obj.name + '_Export', mesh)
    bpy.context.scene.collection.objects.link(export_obj)
    export_obj.matrix_world = Matrix.Identity(4)
    bpy.ops.object.select_all(action='DESELECT')
    export_obj.select_set(True)
    bpy.context.view_layer.objects.active = export_obj
    bpy.context.view_layer.update()
    target = out / (obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(target), use_selection=True, object_types={'MESH'},
                             add_leaf_bones=False, path_mode='AUTO', bake_anim=False)
    mesh.calc_loop_triangles()
    rows.append({'name': obj.name, 'fbx': str(target), 'sha256': sha(target),
                 'source_origin': list(transform.translation), 'triangles': len(mesh.loop_triangles),
                 'uv_layers': [layer.name for layer in mesh.uv_layers],
                 'materials': [mat.name if mat else None for mat in mesh.materials],
                 'generated_connector': suffix == 'TrialPitchNeck'})
    bpy.data.objects.remove(export_obj, do_unlink=True)
    bpy.data.meshes.remove(mesh)
after = sha(source)
if before != after:
    raise RuntimeError('입력 후보가 변경되었습니다')
(out / 'report.json').write_text(json.dumps({'scope': '포신 후보 FBX 3개. 연결부는 시험용 추가 형상. 전장 미적용.',
    'source': str(source), 'source_before': before, 'source_after': after, 'parts': rows},
    ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('OV_WEAPON_PITCH_EXPORT', out)
