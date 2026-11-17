"""원본 초계함의 전체 삼각형을 유지해 별도 UE 임포트용 FBX를 만든다.

Blender 백그라운드에서 실행한다. 원본과 기존 경량 시험본은 수정하지 않는다.
텍스처는 별도 처리하며 이 스크립트는 최종 외형 승인을 뜻하지 않는다.
"""
import hashlib
import json
from pathlib import Path
import bpy
from mathutils import Matrix, Vector

root = Path.home() / 'Documents/OrbitalVanguardAssets'
source = next((root / 'Source').rglob('Meshy_AI_scifi_patrol_corvette_pc14_20261112_image-to-3d-texture.fbx'))
destination = root / 'Prepared/PlayUI_R2_PC14_FullMesh_v1'
destination.mkdir(parents=True, exist_ok=False)
digest = lambda path: hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()
before = digest(source)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source), use_image_search=True)
objects = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
if not objects:
    raise RuntimeError('원본에 메시가 없습니다')
bpy.ops.object.select_all(action='DESELECT')
for obj in objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = objects[0]
if len(objects) > 1:
    bpy.ops.object.join()
hull = bpy.context.active_object
hull.name = 'OV_PC14_FullMesh'
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
minimum = Vector(tuple(min(v[i] for v in hull.bound_box) for i in range(3)))
maximum = Vector(tuple(max(v[i] for v in hull.bound_box) for i in range(3)))
extent = maximum - minimum
if max(range(3), key=lambda i: extent[i]) != 0:
    raise RuntimeError('초계함 장축이 예상한 X축이 아닙니다')
transform = Matrix.Scale(3.5 / max(extent), 4) @ Matrix.Translation(-(minimum + maximum) / 2)
hull.data.transform(transform)
hull.location = Vector()
hull.data.update()
bpy.context.view_layer.update()
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = 1.0
fbx = destination / 'OV_PC14_FullMesh.fbx'
bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'MESH'},
                         apply_unit_scale=True, global_scale=1.0, axis_forward='-Y', axis_up='Z',
                         add_leaf_bones=False, bake_anim=False, path_mode='AUTO', embed_textures=False)
report = {'source': str(source), 'source_sha256': before, 'source_unchanged': digest(source) == before,
          'prepared_fbx': str(fbx), 'prepared_sha256': digest(fbx), 'vertices': len(hull.data.vertices),
          'polygons': len(hull.data.polygons), 'uv_layers': len(hull.data.uv_layers),
          'scope': '원본 전체 메시, 위치·균일 크기 변환만. 텍스처 및 최종 접합은 별도.'}
(destination / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
print('OV_PC14_FULL_MESH', json.dumps(report, ensure_ascii=True))
