"""Blender 백그라운드: 원본 선체에서 27개 상자 충돌 후보를 생성한다.

결과는 검증 전 후보이며 게임에 적용하지 않는다. 입력 FBX는 수정하지 않는다.
"""
from pathlib import Path
import json
import hashlib
import bpy
from mathutils import Vector

root = Path.home() / 'Documents/OrbitalVanguardAssets'
project = Path(__file__).resolve().parents[1]
output = project / 'Saved/Validation/MarkedHullCollisionCandidates.json'
if output.exists():
    raise FileExistsError(f'기존 후보를 덮어쓰지 않음: {output}')
marks = json.loads((project.parents[1] / 'docs/validation/PlayUIR2/JX_무장표시_표면투영후보.json').read_text(encoding='utf8'))['ships']
sources = {'corvette': root / 'Prepared/PlayUI_R2_PC14_FullMesh_v1/OV_PC14_FullMesh.fbx'}
for tag in ('PlayUI_R2_DestroyerFull_v1', 'PlayUI_R2_CarrierAssaultFull_v1', 'PlayUI_R2_MissileDefenseFull_v1',
            'PlayUI_R2_FrigateCruiserRadarFull_v1', 'PlayUI_R2_SupportShipsFull_v1', 'PlayUI_R2_HeavyShipsFull_v1'):
    folder = root / 'Prepared' / tag
    for entry in json.loads((folder / 'report.json').read_text(encoding='utf8')):
        ident = entry['id']
        sources[ident] = folder / ident / ('OV_' + ident + '_FullMesh.fbx')

results = []
for ship in marks:
    ident = ship['id']
    if not ship['candidates']:
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(sources[ident]), use_image_search=False)
    objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if len(objects) != 1:
        raise RuntimeError(f'{ident}: 예상하지 못한 메시 수')
    obj = objects[0]
    points = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    low = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    center, size = (low + high) / 2, high - low
    fit = min(350 / size.x, 150 / size.y, 60 / size.z)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    boxes = []
    hits = 0
    heldout_hits = heldout_covered = 0
    for i in range(27):
        x = -175 + (i + .5) * (350 / 27)
        samples = []
        for y in range(-75, 76, 5):
            pos = Vector((center.x - x / fit, center.y - y / fit, center.z))
            z = []
            for sign in (1, -1):
                start = pos + Vector((0, 0, sign * max(size) * 3))
                hit, point, _, _, _, _ = bpy.context.scene.ray_cast(depsgraph, start, Vector((0, 0, -sign)), distance=max(size)*6)
                z.append((point.z - center.z) * fit if hit else None)
            if None not in z:
                samples.append((y, z[0], z[1]))
        train = [s for s in samples if s[0] % 10 == 0]
        held = [s for s in samples if s[0] % 10 != 0]
        hits += len(samples)
        if not train:
            continue
        ly = max(-75, min(s[0] for s in train) - 5)
        hy = min(75, max(s[0] for s in train) + 5)
        lz = max(-30, min(s[2] for s in train) - 1.5)
        hz = min(30, max(s[1] for s in train) + 1.5)
        boxes.append({'local_center_cm': [x, (ly+hy)/2, (lz+hz)/2], 'half_extent_cm': [175/27, (hy-ly)/2, (hz-lz)/2]})
        heldout_hits += len(held)
        heldout_covered += sum(ly <= y <= hy and lz <= bottom <= hz and lz <= top <= hz for y, top, bottom in held)
    with sources[ident].open('rb') as stream:
        source_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
    results.append({'hull': ident, 'source_file': sources[ident].name, 'source_sha256': source_hash, 'boxes': boxes, 'surface_hit_columns': hits,
                    'heldout_hit_columns': heldout_hits, 'heldout_endpoints_covered': heldout_covered})
    print('OV_COLLISION_CANDIDATE', ident, len(boxes), heldout_covered, '/', heldout_hits, flush=True)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps({'scope': '미적용 27상자 선체 후보. 측면·사선·피격·전투·성능 검증 필요.', 'ships': results}, ensure_ascii=False, indent=2), encoding='utf8')
