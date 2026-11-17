"""원본 전체 초계함 메시를 새 UE 에셋으로 임포트한다. 기존 에셋은 덮어쓰지 않는다."""
from pathlib import Path
import hashlib
import json
import traceback
import unreal

source = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_PC14_FullMesh_v1/OV_PC14_FullMesh.fbx'
asset_path = '/Game/Presentation/Ships/SM_PC14_FullMesh'
out = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation/PC14FullMeshImport.json'
report = {'passed': False, 'source': str(source), 'asset': asset_path, 'checks': [], 'errors': [],
          'scope': '원본 전체 기하만. 기존 축소 텍스처와 임시 포탑 연결부는 별도.'}
def check(name, value, detail=''):
    report['checks'].append({'id': name, 'passed': bool(value), 'detail': str(detail)})
    if not value:
        raise RuntimeError(name + ': ' + str(detail))
try:
    check('SOURCE-EXISTS', source.is_file())
    report['source_sha256'] = hashlib.file_digest(source.open('rb'), 'sha256').hexdigest()
    check('NO-OVERWRITE', not unreal.EditorAssetLibrary.does_asset_exist(asset_path))
    options = unreal.FbxImportUI()
    options.set_editor_property('import_mesh', True)
    options.set_editor_property('import_as_skeletal', False)
    options.set_editor_property('automated_import_should_detect_type', False)
    options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)
    data = options.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('convert_scene', True)
    data.set_editor_property('convert_scene_unit', True)
    task = unreal.AssetImportTask()
    for key, value in [('filename', str(source)), ('destination_path', '/Game/Presentation/Ships'),
                       ('destination_name', 'SM_PC14_FullMesh'), ('replace_existing', False),
                       ('automated', True), ('save', True), ('factory', unreal.FbxFactory()), ('options', options)]:
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
    check('STATIC-MESH', isinstance(mesh, unreal.StaticMesh))
    editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    report['triangles'] = mesh.get_num_triangles(0)
    report['vertices'] = mesh.get_num_vertices(0)
    report['uv_channels'] = editor.get_num_uv_channels(mesh, 0)
    report['simple_collisions'] = editor.get_simple_collision_count(mesh)
    bounds = mesh.get_bounding_box()
    report['bounds_cm'] = [float(getattr(bounds.max, a) - getattr(bounds.min, a)) for a in ('x','y','z')]
    check('FULL-TRIANGLES', report['triangles'] == 300142, report['triangles'])
    check('UV-PRESERVED', report['uv_channels'] >= 1)
    check('NO-COLLISION', report['simple_collisions'] == 0)
    check('EXPECTED-SCALE', abs(report['bounds_cm'][0] - 350) < 1, report['bounds_cm'])
    check('SAVED', unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False))
    asset = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())) / 'Presentation/Ships/SM_PC14_FullMesh.uasset'
    report['asset_bytes'] = asset.stat().st_size
    report['asset_sha256'] = hashlib.file_digest(asset.open('rb'), 'sha256').hexdigest()
    report['passed'] = True
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    unreal.SystemLibrary.quit_editor()
