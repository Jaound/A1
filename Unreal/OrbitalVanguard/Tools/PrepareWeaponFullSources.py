"""원본 무장 9종의 전체 메시·내장 텍스처를 별도 폴더에 보존한다.

이 단계는 관절 분리·가동·게임 적용을 뜻하지 않는다. 원본을 수정하지 않는다.
Blender: --background --factory-startup --python PrepareWeaponFullSources.py
"""
import hashlib
import json
from pathlib import Path
import bpy

root = Path.home() / 'Documents/OrbitalVanguardAssets'
reports = root / 'Reports'
destination = root / 'Prepared/PlayUI_R2_WeaponFullSources_v1'
if destination.exists():
    raise FileExistsError(f'기존 준비본 보호: {destination}')
ids = {'naval_gun_bf120','laser_turret_lb30','ciws_gatling_6','heavy_cannon_shc400',
       'railgun_rg200','plasma_heavy_cannon','missile_pod_mm24','missile_pod_mm48','ciws_beam_b'}
models = {}
for filename in ('PlayUI_v1_20261117b_Blender.json', 'PlayUI_R2_20261117_inspect_Blender.json'):
    for model in json.loads((reports / filename).read_text(encoding='utf8'))['models']:
        if model['id'] in ids:
            models[model['id']] = model
if set(models) != ids:
    raise RuntimeError(f'9종 원본 목록 확인 실패: {ids-set(models)}')
destination.mkdir(parents=True)
rows = []
for ident in sorted(ids):
    model = models[ident]
    source = Path(model['source_fbx'])
    if not source.is_file():
        raise FileNotFoundError(source)
    def sha(path):
        with path.open('rb') as stream:
            return hashlib.file_digest(stream, 'sha256').hexdigest()
    before = sha(source)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source), use_image_search=True)
    objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not objects:
        raise RuntimeError(f'{ident}: 원본 메시 없음')
    out = destination / ident
    out.mkdir()
    textures = []
    for image in bpy.data.images:
        if not image.packed_file:
            continue
        safe = ''.join(c if c.isalnum() or c in '._-' else '_' for c in image.name)
        suffix = Path(image.filepath).suffix.lower() or '.png'
        target = out / f'{safe}{suffix}'
        if target.exists():
            raise FileExistsError(target)
        target.write_bytes(bytes(image.packed_file.data))
        textures.append({'file': target.name, 'sha256': sha(target), 'size': list(image.size)})
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    fbx = out / f'OV_{ident}_SourceFull.fbx'
    bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'MESH'},
                             apply_unit_scale=True, global_scale=1.0, axis_forward='-Y', axis_up='Z',
                             add_leaf_bones=False, bake_anim=False, path_mode='AUTO', embed_textures=False)
    rows.append({'id': ident, 'source_fbx': str(source), 'source_sha256': before,
                 'source_unchanged': sha(source) == before, 'prepared_fbx': str(fbx), 'prepared_sha256': sha(fbx),
                 'meshes': [{'name': o.name, 'vertices': len(o.data.vertices), 'polygons': len(o.data.polygons),
                             'uv_layers': len(o.data.uv_layers)} for o in objects], 'textures': textures})
    (destination / 'report.json').write_text(json.dumps({'scope': '원본 전체 메시·텍스처 복사. 관절 분리/UE/전장 미적용.', 'models': rows},
                                                        ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print('OV_WEAPON_SOURCE', ident, len(objects), len(textures), flush=True)
