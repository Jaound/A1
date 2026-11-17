"""Create a new UE material from reduced copies of the PC14 source maps.

Runs in a separate editor. Never replaces existing assets or source files.
The 1024px material is a trial, not final source-quality approval.
"""
import hashlib
import json
import os
from pathlib import Path
import traceback

import unreal

root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_PC14_Textures1024'
destination = '/Game/Presentation/PC14Trial'
saved = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation/PC14TrialMaterial.json'
report = {'passed': False, 'scope': '1024px source-derived trial material; no final model approval',
          'source_modified': False, 'destination': destination, 'checks': [], 'errors': []}

def check(name, condition, detail=''):
    report['checks'].append({'id': name, 'passed': bool(condition), 'detail': str(detail)})
    if not condition:
        raise AssertionError(name + ': ' + str(detail))

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

try:
    files = [('Image_0.png', 'T_PC14_Base_Trial', 'base'),
             ('Image_2.png', 'T_PC14_Normal_Trial', 'normal'),
             ('texture_0_roughness.png', 'T_PC14_Roughness_Trial', 'roughness'),
             ('texture_0_metallic.png', 'T_PC14_Metallic_Trial', 'metallic')]
    material_name = 'M_PC14_Trial'
    for filename, asset, _ in files:
        check('SOURCE-' + asset, (root / filename).is_file())
        check('NO-OVERWRITE-' + asset, not unreal.EditorAssetLibrary.does_asset_exist(destination + '/' + asset))
    check('NO-OVERWRITE-MATERIAL', not unreal.EditorAssetLibrary.does_asset_exist(destination + '/' + material_name))
    hashes = {name: digest(root / name) for name, _, _ in files}
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    textures = {}
    for filename, asset, kind in files:
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', str(root / filename))
        task.set_editor_property('destination_path', destination)
        task.set_editor_property('destination_name', asset)
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', False)
        task.set_editor_property('save', True)
        task.set_editor_property('factory', unreal.TextureFactory())
        tools.import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(destination + '/' + asset)
        check('IMPORTED-' + asset, isinstance(texture, unreal.Texture2D))
        if kind != 'base':
            texture.set_editor_property('srgb', False)
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if kind == 'normal' else unreal.TextureCompressionSettings.TC_MASKS)
        check('SAVE-' + asset, unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False))
        textures[kind] = texture
    material = tools.create_asset(material_name, destination, unreal.Material, unreal.MaterialFactoryNew())
    check('MATERIAL-CREATED', isinstance(material, unreal.Material))
    library = unreal.MaterialEditingLibrary
    nodes = {}
    for index, kind in enumerate(('base', 'normal', 'roughness', 'metallic')):
        node = library.create_material_expression(material, unreal.MaterialExpressionTextureSample, -700, index * 250)
        node.set_editor_property('texture', textures[kind])
        node.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind == 'normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if kind != 'base' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        nodes[kind] = node
    tint = library.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, -220)
    tint.set_editor_property('parameter_name', 'FactionTint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    multiply = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, -100)
    check('CONNECT-BASE-TINT', library.connect_material_expressions(nodes['base'], 'RGB', multiply, 'A') and library.connect_material_expressions(tint, 'RGB', multiply, 'B'))
    check('CONNECT-BASE', library.connect_material_property(multiply, '', unreal.MaterialProperty.MP_BASE_COLOR))
    for kind, prop in [('normal', unreal.MaterialProperty.MP_NORMAL), ('roughness', unreal.MaterialProperty.MP_ROUGHNESS), ('metallic', unreal.MaterialProperty.MP_METALLIC)]:
        check('CONNECT-' + kind, library.connect_material_property(nodes[kind], 'RGB' if kind == 'normal' else 'R', prop))
    library.recompile_material(material)
    check('SAVE-MATERIAL', unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False))
    check('SOURCE-UNCHANGED', all(digest(root / name) == hashes[name] for name, _, _ in files))
    report['source_sha256'] = hashes
    report['assets'] = [destination + '/' + asset for _, asset, _ in files] + [destination + '/' + material_name]
    report['passed'] = True
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    saved.parent.mkdir(parents=True, exist_ok=True)
    saved.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
