"""분리 후보 한 종을 UE의 별도 폴더로 반입하고 시험 LOD를 생성한다.

전장 기본 무장이나 기존 원본 애셋을 변경하지 않는다. 결과는 별도 검증이 필요하다.
"""
import hashlib
import json
import os
from pathlib import Path
import traceback

import unreal

ident = os.environ.get('OV_WEAPON_YAW_ID', '')
if not ident or not ident.replace('_', '').isalnum():
    raise ValueError('OV_WEAPON_YAW_ID가 필요합니다')
root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponYawExports_v1' / ident
run = os.environ.get('OV_WEAPON_YAW_RUN', 'first')
if not run.replace('_', '').isalnum():
    raise ValueError('잘못된 검사 식별자')
report_path = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation' / ('WeaponYawImport_' + ident + '_' + run + '.json')
dest = '/Game/Presentation/WeaponsYawTrial/' + ident
if report_path.exists() or unreal.EditorAssetLibrary.does_directory_exist(dest):
    raise RuntimeError('기존 결과 또는 에셋을 덮어쓰지 않습니다')
report = {'id': ident, 'passed': False, 'scope': '원본 재질을 참조하는 회전 후보 격리 반입 및 시험 LOD. 전장 미적용.',
          'parts': [], 'errors': []}

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

try:
    source = json.loads((root / 'report.json').read_text(encoding='utf8'))
    reimport = json.loads((root / 'reimport.json').read_text(encoding='utf8'))
    require(reimport['passed'] and len(source['parts']) == 2, 'FBX 재반입 검사 필요')
    material = unreal.EditorAssetLibrary.load_asset('/Game/Presentation/WeaponsFull/' + ident + '/M_Source')
    require(isinstance(material, unreal.MaterialInterface), '원본 UE 재질이 없습니다')
    tool = unreal.AssetToolsHelpers.get_asset_tools()
    editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for part in source['parts']:
        path = Path(part['fbx'])
        require(sha(path) == part['sha256'], 'FBX 해시 불일치')
        name = 'SM_' + ('Base' if part['name'].endswith('_BaseTrial') else 'Yaw')
        options = unreal.FbxImportUI()
        for key, value in [('import_mesh', True), ('import_as_skeletal', False),
                           ('automated_import_should_detect_type', False),
                           ('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH),
                           ('import_materials', False), ('import_textures', False)]:
            options.set_editor_property(key, value)
        settings = options.get_editor_property('static_mesh_import_data')
        for key, value in [('combine_meshes', True), ('auto_generate_collision', False), ('remove_degenerates', False),
                           ('generate_lightmap_u_vs', False), ('convert_scene', True), ('convert_scene_unit', True)]:
            settings.set_editor_property(key, value)
        task = unreal.AssetImportTask()
        for key, value in [('filename', str(path)), ('destination_path', dest), ('destination_name', name),
                           ('replace_existing', False), ('automated', True), ('save', False),
                           ('factory', unreal.FbxFactory()), ('options', options)]:
            task.set_editor_property(key, value)
        tool.import_asset_tasks([task])
        mesh = unreal.EditorAssetLibrary.load_asset(dest + '/' + name)
        require(isinstance(mesh, unreal.StaticMesh), 'UE 메시 반입 실패: ' + name)
        original = mesh.get_num_triangles(0)
        uv_count = editor.get_num_uv_channels(mesh, 0)
        require(original == part['triangles'] and uv_count > 0,
                '삼각형 또는 UV 불일치: %s UE=%d source=%d UV=%d' % (name, original, part['triangles'], uv_count))
        require(editor.get_simple_collision_count(mesh) == 0, '표시 메시에는 충돌이 없어야 합니다')
        mesh.set_material(0, material)
        lods = unreal.StaticMeshReductionOptions()
        lods.set_editor_property('auto_compute_lod_screen_size', False)
        levels = []
        for ratio, screen in [(1., 1.), (.05, .12), (.01, .03)]:
            level = unreal.StaticMeshReductionSettings()
            level.set_editor_property('percent_triangles', ratio)
            level.set_editor_property('screen_size', screen)
            levels.append(level)
        lods.set_editor_property('reduction_settings', levels)
        require(editor.set_lods(mesh, lods) == 3, '시험 LOD 생성 실패')
        counts = [mesh.get_num_triangles(i) for i in range(3)]
        require(counts[0] == original and 0 < counts[2] < counts[1] < counts[0], 'LOD 수량 검증 실패')
        require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), '메시 저장 실패')
        report['parts'].append({'asset': dest + '/' + name, 'fbx_sha256': part['sha256'],
                                'source_origin': part['source_origin'], 'triangles': counts,
                                'source_material': material.get_path_name()})
    report['passed'] = len(report['parts']) == 2
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    unreal.SystemLibrary.quit_editor()
