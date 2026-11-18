"""입증된 작은 절단 파편을 원본 복사본의 고정부로 되돌리는 시험이다."""
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy

args = sys.argv[sys.argv.index('--') + 1:]
if len(args) != 2:
    raise ValueError('후보 ID와 새 출력 폴더가 필요합니다')
ident, output = args
source, out = Path(bpy.data.filepath).resolve(), Path(output).resolve()
if not source.is_file() or out.exists():
    raise RuntimeError('입력이 없거나 기존 출력을 덮어쓰게 됩니다')

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

before = sha(source)
metadata = json.loads((source.parent / 'report.json').read_text(encoding='utf8'))
body = bpy.data.objects.get(ident + '_YawBody')
barrel = bpy.data.objects.get(ident + '_PitchTrial')
neck = bpy.data.objects.get(ident + '_TrialPitchNeck')
if not body or not barrel or not neck or neck.parent != barrel:
    raise RuntimeError('예상한 원본 후보 계층이 없습니다')
before_objects = set(bpy.context.scene.objects)
bpy.ops.object.select_all(action='DESELECT')
barrel.select_set(True)
bpy.context.view_layer.objects.active = barrel
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.separate(type='LOOSE')
bpy.ops.object.mode_set(mode='OBJECT')
separated = [barrel] + [obj for obj in bpy.context.scene.objects if obj not in before_objects and obj.type == 'MESH']
separated.sort(key=lambda obj: len(obj.data.vertices), reverse=True)
if len(separated) != 3 or any(len(obj.data.vertices) >= 500 for obj in separated[1:]):
    raise RuntimeError('예상한 2개 소형 파편이 아닙니다. 원본은 변경하지 않았습니다.')
main = separated[0]
if main != barrel:
    world = neck.matrix_world.copy()
    neck.parent = main
    neck.matrix_world = world
    barrel.name = ident + '_Fragment'
    main.name = ident + '_PitchTrial'
barrel = main
fragment_counts = [len(obj.data.vertices) for obj in separated[1:]]
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
for obj in separated[1:]:
    obj.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.join()
out.mkdir(parents=True)
for angle in (0, 15, -15, 30, -30):
    barrel.rotation_euler.y = math.radians(angle)
    bpy.context.view_layer.update()
    bpy.context.scene.render.filepath = str(out / ('pitch_%+03d.png' % angle))
    bpy.ops.render.render(write_still=True)
barrel.rotation_euler.y = 0
bpy.ops.wm.save_as_mainfile(filepath=str(out / (ident + '_PitchTrial.blend')))
after = sha(source)
if before != after:
    raise RuntimeError('입력 파일이 변경되었습니다')
metadata.update({'source': str(source), 'source_before': before, 'source_after': after,
                 'reattached_fragment_vertices': fragment_counts,
                 'status': 'experimental reattachment; intersections, UV and visual fit unapproved'})
(out / 'report.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('OV_PITCH_FRAGMENT_TRIAL', out)
