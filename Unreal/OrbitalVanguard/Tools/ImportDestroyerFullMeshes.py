"""백터·바스티온 원본 메시와 맵을 새 UE 에셋으로 반입한다. 덮어쓰지 않는다."""
from pathlib import Path
import json
import os
import traceback
import unreal

root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared' / os.environ.get('OV_FULL_SHIP_TAG', 'PlayUI_R2_DestroyerFull_v1')
source_report = json.loads((root / 'report.json').read_text(encoding='utf8'))
output = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation' / (os.environ.get('OV_FULL_SHIP_TAG', 'DestroyerFull') + 'Import.json')
report = {'passed': False, 'ships': [], 'errors': []}
def check(name, value):
    if not value: raise RuntimeError(name)
try:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    lib = unreal.MaterialEditingLibrary
    for src in source_report:
        ident = src['id']
        destination = '/Game/Presentation/Ships/' + ident.title() + 'Full'
        names = ['SM_Hull', 'T_Base', 'T_Normal', 'T_Roughness', 'T_Metallic', 'M_Readable']
        for name in names:
            check('NO-OVERWRITE ' + destination + '/' + name, not unreal.EditorAssetLibrary.does_asset_exist(destination + '/' + name))
        options = unreal.FbxImportUI()
        for key, value in [('import_mesh', True), ('import_as_skeletal', False), ('automated_import_should_detect_type', False),
                           ('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH), ('import_materials', False), ('import_textures', False)]:
            options.set_editor_property(key, value)
        settings = options.get_editor_property('static_mesh_import_data')
        for key, value in [('combine_meshes', True), ('auto_generate_collision', False), ('generate_lightmap_u_vs', False),
                           ('convert_scene', True), ('convert_scene_unit', True)]: settings.set_editor_property(key, value)
        def task(filename, name, factory, extra=None):
            item = unreal.AssetImportTask()
            for key, value in [('filename', str(filename)), ('destination_path', destination), ('destination_name', name),
                               ('replace_existing', False), ('automated', True), ('save', True), ('factory', factory)]:
                item.set_editor_property(key, value)
            if extra: item.set_editor_property('options', extra)
            tools.import_asset_tasks([item])
            return unreal.EditorAssetLibrary.load_asset(destination + '/' + name)
        mesh = task(root / ident / ('OV_' + ident + '_FullMesh.fbx'), 'SM_Hull', unreal.FbxFactory(), options)
        check('MESH ' + ident, isinstance(mesh, unreal.StaticMesh))
        triangles = mesh.get_num_triangles(0)
        check('TRIANGLES ' + ident, triangles == src['polygons'])
        check('UV ' + ident, mesh_editor.get_num_uv_channels(mesh, 0) >= 1)
        check('NO COLLISION ' + ident, mesh_editor.get_simple_collision_count(mesh) == 0)
        textures = {}
        for kind, filename, name in [('base','Image_0.png','T_Base'),('normal','Image_2.png','T_Normal'),
                                      ('roughness','texture_0_roughness.png','T_Roughness'),('metallic','texture_0_metallic.png','T_Metallic')]:
            texture = task(root / ident / 'Textures' / filename, name, unreal.TextureFactory())
            check('TEXTURE ' + ident + ' ' + kind, isinstance(texture, unreal.Texture2D))
            if kind != 'base':
                texture.set_editor_property('srgb', False)
                texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if kind == 'normal' else unreal.TextureCompressionSettings.TC_MASKS)
            check('SAVE TEXTURE ' + kind, unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False))
            textures[kind] = texture
        material = tools.create_asset('M_Readable', destination, unreal.Material, unreal.MaterialFactoryNew())
        check('MATERIAL', isinstance(material, unreal.Material))
        nodes = {}
        for index, kind in enumerate(('base','normal','roughness','metallic')):
            node = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -700, index * 220)
            node.set_editor_property('texture', textures[kind])
            node.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind == 'normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if kind != 'base' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
            nodes[kind] = node
        tint = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, -220)
        tint.set_editor_property('parameter_name', 'FactionTint')
        tint.set_editor_property('default_value', unreal.LinearColor(1,1,1,1))
        tinted = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, -80)
        check('TINT', lib.connect_material_expressions(nodes['base'],'RGB',tinted,'A') and lib.connect_material_expressions(tint,'RGB',tinted,'B'))
        check('BASE', lib.connect_material_property(tinted,'',unreal.MaterialProperty.MP_BASE_COLOR))
        for kind, prop in [('normal',unreal.MaterialProperty.MP_NORMAL),('roughness',unreal.MaterialProperty.MP_ROUGHNESS),('metallic',unreal.MaterialProperty.MP_METALLIC)]:
            check('CONNECT '+kind,lib.connect_material_property(nodes[kind],'RGB' if kind == 'normal' else 'R',prop))
        fill = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -350, -300)
        fill.set_editor_property('r', 1.2)
        emission = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -100, -180)
        check('FILL',lib.connect_material_expressions(tinted,'',emission,'A') and lib.connect_material_expressions(fill,'',emission,'B') and lib.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR))
        lib.recompile_material(material)
        check('SAVE MATERIAL',unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False))
        report['ships'].append({'id': ident, 'triangles': triangles, 'mesh': destination+'/SM_Hull',
            'material': destination+'/M_Readable', 'source_sha256': src['source_sha256'], 'images': src['textures'],
            'forward_direction_validated': False, 'visibility_fill': 1.2})
    report['passed'] = True
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    unreal.SystemLibrary.quit_editor()
