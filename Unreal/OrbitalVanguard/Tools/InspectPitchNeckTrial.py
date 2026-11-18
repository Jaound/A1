"""시험용 포신 연결부와 본체·받침의 표면 교차를 각도별로 측정한다.

덮개와 같은 부품 내부의 의도된 접합은 제외한다. 연속 각도·재질·실전 검증은 아니다.
"""
import hashlib
import json
import math
from pathlib import Path
import sys

import bmesh
import bpy
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index('--') + 1:]
if len(args) not in (2, 3):
    raise ValueError('후보 ID와 새 결과 경로 및 선택 간격이 필요합니다')
ident, out_arg = args[:2]
clearance = float(args[2]) if len(args) == 3 else 0.0
if not 0 <= clearance <= .05:
    raise ValueError('시험 간격이 유효하지 않습니다')
source = Path(bpy.data.filepath).resolve()
out = Path(out_arg).resolve()
if not source.is_file() or out.exists():
    raise RuntimeError('입력 파일이 없거나 결과가 이미 존재합니다')

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

objects = {name: bpy.data.objects.get(ident + '_' + suffix) for name, suffix in
           [('base', 'BaseTrial'), ('body', 'YawBody'), ('barrel', 'PitchTrial'), ('neck', 'TrialPitchNeck')]}
if any(obj is None for obj in objects.values()) or objects['neck'].parent != objects['barrel']:
    raise RuntimeError('필요한 후보 부품 또는 계층이 없습니다')

def tree(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    caps = [f for f in bm.faces if f.material_index < len(obj.data.materials)
            and obj.data.materials[f.material_index]
            and obj.data.materials[f.material_index].name.startswith(('TrialPitchCutCap', 'TrialCutCap'))]
    if caps:
        bmesh.ops.delete(bm, geom=caps, context='FACES_ONLY')
    bm.transform(obj.matrix_world)
    result = BVHTree.FromBMesh(bm, epsilon=0.0)
    bm.free()
    return result

before = sha(source)
objects['body'].location.z += clearance
bpy.context.view_layer.update()
base_tree = tree(objects['base'])
samples = []
for yaw in (-180, -90, 0, 90, 180):
    objects['body'].rotation_euler.z = math.radians(yaw)
    bpy.context.view_layer.update()
    body_tree = tree(objects['body'])
    body_base = len(base_tree.overlap(body_tree))
    for pitch in range(-15, 91, 15):
        objects['barrel'].rotation_euler.y = math.radians(pitch)
        bpy.context.view_layer.update()
        barrel_tree, neck_tree = tree(objects['barrel']), tree(objects['neck'])
        samples.append({'yaw': yaw, 'pitch': pitch, 'body_base': body_base,
                        'barrel_body': len(body_tree.overlap(barrel_tree)),
                        'barrel_base': len(base_tree.overlap(barrel_tree)),
                        'neck_body': len(body_tree.overlap(neck_tree)),
                        'neck_base': len(base_tree.overlap(neck_tree))})
after = sha(source)
if before != after:
    raise RuntimeError('입력 후보가 변경되었습니다')
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({'source_before': before, 'source_after': after, 'trial_clearance': clearance, 'samples': samples,
                           'scope': '상부 장착의 이산 각도 표면 교차만 측정. 모든 각도·원본 UV·게임 미검증.'},
                          ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('OV_PITCH_NECK_INSPECTION', out)
