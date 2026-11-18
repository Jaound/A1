"""CIWS 포신 후보를 신규 UE 폴더에 격리 반입한다. 전장에는 적용하지 않는다."""
import hashlib
import json
import os
from pathlib import Path
import traceback

import unreal

ident = os.environ.get('OV_WEAPON_PITCH_ID', '')
run = os.environ.get('OV_WEAPON_PITCH_RUN', 'first')
if not ident or not ident.replace('_', '').isalnum() or not run.replace('_', '').isalnum():
    raise ValueError('후보 ID와 실행 ID가 필요합니다')
version = os.environ.get('OV_WEAPON_PITCH_SOURCE_VERSION', 'v2')
if not version.replace('_', '').isalnum():
    raise ValueError('잘못된 FBX 버전')
root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared' / ('PlayUI_R2_WeaponPitchExports_' + version) / ident
dest = '/Game/Presentation/WeaponsPitchTrial/' + ident
out = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation' / ('WeaponPitchImport_' + ident + '_' + run + '.json')
if out.exists() or unreal.EditorAssetLibrary.does_directory_exist(dest):
    raise RuntimeError('기존 보고서 또는 자산을 덮어쓰지 않습니다')
report = {'id': ident, 'passed': False, 'scope': '포신 후보 격리 반입. 전장 미적용; 연결부는 시험용 추가 형상.',
          'parts': [], 'errors': []}

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

try:
    source = json.loads((root / 'report.json').read_text(encoding='utf8'))
    verification = json.loads((root / 'reimport.json').read_text(encoding='utf8'))
    require(verification['passed'] and len(source['parts']) == 3, '세 부품 재반입 검사가 필요합니다')
    require([p['generated_connector'] for p in source['parts']] == [False, False, True], '원본 부품과 시험 연결부 분류 오류')
    require([p['name'] for p in source['parts']] == [ident + '_YawBody', ident + '_PitchTrial', ident + '_TrialPitchNeck'],
            '포신 부품 순서 오류')
    original = unreal.EditorAssetLibrary.load_asset('/Game/Presentation/WeaponsFull/' + ident + '/M_Source')
    trial = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial')
    require(isinstance(original, unreal.MaterialInterface) and trial, '원본 또는 시험 재질 없음')
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for part, name in zip(source['parts'], ('SM_Body', 'SM_Barrel', 'SM_TrialNeck')):
        file = Path(part['fbx'])
        require(sha(file) == part['sha256'], '입력 FBX 해시 불일치')
        options = unreal.FbxImportUI()
        for key, value in [('import_mesh', True), ('import_as_skeletal', False), ('automated_import_should_detect_type', False),
                           ('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH), ('import_materials', False), ('import_textures', False)]:
            options.set_editor_property(key, value)
        settings = options.get_editor_property('static_mesh_import_data')
        for key, value in [('combine_meshes', True), ('auto_generate_collision', False), ('remove_degenerates', False),
                           ('generate_lightmap_u_vs', False), ('convert_scene', True), ('convert_scene_unit', True)]:
            settings.set_editor_property(key, value)
        task = unreal.AssetImportTask()
        for key, value in [('filename', str(file)), ('destination_path', dest), ('destination_name', name),
                           ('replace_existing', False), ('automated', True), ('save', False),
                           ('factory', unreal.FbxFactory()), ('options', options)]:
            task.set_editor_property(key, value)
        tools.import_asset_tasks([task])
        mesh = unreal.EditorAssetLibrary.load_asset(dest + '/' + name)
        require(isinstance(mesh, unreal.StaticMesh), '메시 반입 실패: ' + name)
        count = mesh.get_num_triangles(0)
        require(count == part['triangles'] and editor.get_num_uv_channels(mesh, 0) > 0, '삼각형 또는 UV 불일치: ' + name)
        require(editor.get_simple_collision_count(mesh) == 0, '표시 메시 충돌이 있습니다')
        mesh.set_material(0, trial if part['generated_connector'] else original)
        counts = [count]
        if count > 5000:
            options = unreal.StaticMeshReductionOptions()
            options.set_editor_property('auto_compute_lod_screen_size', False)
            levels = []
            for ratio, screen in [(1., 1.), (.05, .12), (.01, .03)]:
                level = unreal.StaticMeshReductionSettings()
                level.set_editor_property('percent_triangles', ratio)
                level.set_editor_property('screen_size', screen)
                levels.append(level)
            options.set_editor_property('reduction_settings', levels)
            require(editor.set_lods(mesh, options) == 3, '시험 LOD 생성 실패')
            counts = [mesh.get_num_triangles(i) for i in range(3)]
            require(counts[0] == count and 0 < counts[2] < counts[1] < counts[0], 'LOD 수량 오류')
        require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), '자산 저장 실패')
        report['parts'].append({'asset': dest + '/' + name, 'triangles': counts, 'source_origin': part['source_origin'],
                                'fbx_sha256': part['sha256'], 'generated_connector': part['generated_connector']})
    report['passed'] = len(report['parts']) == 3
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    unreal.SystemLibrary.quit_editor()
