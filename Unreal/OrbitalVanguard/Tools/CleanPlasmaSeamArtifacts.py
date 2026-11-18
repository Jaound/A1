"""원본은 보존하고 시험 복사본의 식별된 미세 절단 파편 3개만 제거한다."""
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:]
if len(args) != 1:
    raise ValueError('새 출력 폴더가 필요합니다')
ident = 'plasma_heavy_cannon'
source, out = Path(bpy.data.filepath).resolve(), Path(args[0]).resolve()
if not source.is_file() or out.exists():
    raise RuntimeError('입력이 없거나 출력을 덮어쓰게 됩니다')

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

before = sha(source)
metadata = json.loads((source.parent / 'report.json').read_text(encoding='utf8'))
body = bpy.data.objects.get(ident + '_YawBody')
barrel = bpy.data.objects.get(ident + '_PitchTrial')
if not body or not barrel or barrel.parent != body:
    raise RuntimeError('예상한 후보 계층이 없습니다')
old_objects = set(bpy.context.scene.objects)
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.separate(type='LOOSE'); bpy.ops.object.mode_set(mode='OBJECT')
parts = [body] + [obj for obj in bpy.context.scene.objects if obj not in old_objects and obj.type == 'MESH']
parts.sort(key=lambda obj: len(obj.data.vertices), reverse=True)
if len(parts) != 7 or parts[0] != body:
    raise RuntimeError('예상한 7개 분리 성분이 아닙니다')
artifacts, keep = [], []
for obj in parts[1:]:
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    x = sum(point.x for point in corners) / len(corners)
    y = sum(point.y for point in corners) / len(corners)
    (artifacts if len(obj.data.vertices) <= 150 and -.2 < x < 0 and abs(y) > .55 else keep).append(obj)
if sorted(len(obj.data.vertices) for obj in artifacts) != [11, 67, 81]:
    raise RuntimeError('검사된 절단 파편 3개와 일치하지 않습니다')
counts = [len(obj.data.vertices) for obj in artifacts]
for obj in artifacts:
    bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
for obj in keep:
    obj.select_set(True)
bpy.context.view_layer.objects.active = body
if keep:
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
                 'removed_seam_artifact_vertices': counts,
                 'status': 'experimental seam-artifact cleanup on copy; visual and gameplay unapproved'})
(out / 'report.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('OV_PLASMA_SEAM_CLEANUP', out)
