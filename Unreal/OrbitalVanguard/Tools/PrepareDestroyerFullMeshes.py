"""백터·바스티온 원본 전체 메시와 원본 해상도 맵을 별도 폴더에 추출한다.

방향·장착점 검증 전 준비 단계이며 기존 원본 및 시험본을 수정하지 않는다.
"""
import hashlib
import json
import os
from pathlib import Path
import bpy
from mathutils import Matrix, Vector

root = Path.home() / 'Documents/OrbitalVanguardAssets'
config_path = os.environ.get('OV_FULL_SHIP_CONFIG')
config = json.loads(Path(config_path).read_text(encoding='utf8')) if config_path else {
    'tag': 'PlayUI_R2_DestroyerFull_v1',
    'ships': [{'id':'vector', 'folder':'백터급 구축함', 'pattern':'Meshy_AI_model_20261112_texture.fbx'},
              {'id':'bastion', 'folder':'바스티온급 구축함', 'pattern':'Meshy_AI_model_20261112_texture.fbx'}]}
output = root / 'Prepared' / config['tag']
output.mkdir(parents=True, exist_ok=False)
digest = lambda p: hashlib.file_digest(p.open('rb'), 'sha256').hexdigest()
rows = []
for entry in config['ships']:
    identifier, folder = entry['id'], entry['folder']
    candidates = list((root / 'Source' / folder).rglob(entry['pattern']))
    if len(candidates) != 1:
        raise RuntimeError(f'{identifier}: 원본 FBX {len(candidates)}개')
    source = candidates[0]
    source_hash = digest(source)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source), use_image_search=True)
    objects = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
    if not objects:
        raise RuntimeError(f'{identifier}: 메시 없음')
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects: obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    if len(objects) > 1: bpy.ops.object.join()
    hull = bpy.context.active_object
    hull.name = 'OV_' + identifier + '_FullMesh'
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    minimum = Vector(tuple(min(v[i] for v in hull.bound_box) for i in range(3)))
    maximum = Vector(tuple(max(v[i] for v in hull.bound_box) for i in range(3)))
    extent = maximum - minimum
    if max(range(3), key=lambda i: extent[i]) != 0:
        raise RuntimeError(f'{identifier}: 예상과 다른 장축')
    # Preserve source orientation here. Forward direction is validated separately.
    hull.data.transform(Matrix.Scale(3.5 / max(extent), 4) @ Matrix.Translation(-(minimum + maximum) / 2))
    hull.location = Vector()
    hull.data.update()
    bpy.context.view_layer.update()
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1.0
    destination = output / identifier
    destination.mkdir()
    textures = destination / 'Textures'
    textures.mkdir()
    images = []
    for image in bpy.data.images:
        if image.source != 'FILE' or not image.packed_file: continue
        name = image.name if image.name.lower().endswith('.png') else image.name + '.png'
        target = textures / name
        if target.exists(): raise RuntimeError(f'중복 이미지: {target}')
        image.filepath_raw = str(target)
        image.file_format = 'PNG'
        image.save()
        images.append({'name': name, 'size': list(image.size), 'sha256': digest(target)})
    fbx = destination / ('OV_' + identifier + '_FullMesh.fbx')
    bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'MESH'},
        apply_unit_scale=True, global_scale=1.0, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
        bake_anim=False, path_mode='AUTO', embed_textures=False)
    rows.append({'id': identifier, 'source': str(source), 'source_sha256': source_hash,
        'source_unchanged': digest(source) == source_hash, 'prepared_sha256': digest(fbx),
        'vertices': len(hull.data.vertices), 'polygons': len(hull.data.polygons), 'uv_layers': len(hull.data.uv_layers),
        'source_extent': list(extent), 'textures': images, 'forward_direction_validated': False})
    (output / 'report.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf8')
    print('OV_SHIP_FULL', identifier, len(hull.data.polygons), flush=True)
