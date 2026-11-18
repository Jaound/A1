"""SpaceSkybox, Asteroid, BlackHole, HazardZone 3D 모델 및 PBR/Emissive 머티리얼 반입."""
from pathlib import Path
import json
import traceback
import unreal

report_path = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation' / 'ImportSpaceEnvironmentAssets.json'
report = {'passed': False, 'imported': [], 'checks': [], 'errors': []}


def check(name, value):
    report['checks'].append({'id': name, 'passed': bool(value)})
    if not value:
        raise RuntimeError('CHECK FAILED: ' + name)


def enable_instanced_usage(mat):
    for prop in ('b_used_with_instanced_static_meshes', 'used_with_instanced_static_meshes'):
        try:
            mat.set_editor_property(prop, True)
            if mat.get_editor_property(prop):
                return True
        except Exception:
            pass
    return False


def build_readable_material(lib, mat, textures, fill_strength=1.45, two_sided=True, use_instanced=True):
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property('two_sided', two_sided)
    if use_instanced:
        enable_instanced_usage(mat)

    nodes = {}
    for index, kind in enumerate(('base', 'normal', 'roughness', 'metallic')):
        node = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -700, index * 220)
        node.set_editor_property('texture', textures[kind])
        node.set_editor_property(
            'sampler_type',
            unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind == 'normal'
            else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if kind != 'base'
            else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
        )
        nodes[kind] = node

    tint = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, -220)
    tint.set_editor_property('parameter_name', 'FactionTint')
    tint.set_editor_property('default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    tinted = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, -80)
    check(
        'TINT-' + mat.get_name(),
        lib.connect_material_expressions(nodes['base'], 'RGB', tinted, 'A')
        and lib.connect_material_expressions(tint, 'RGB', tinted, 'B')
    )
    check('BASE-' + mat.get_name(), lib.connect_material_property(tinted, '', unreal.MaterialProperty.MP_BASE_COLOR))

    for kind, prop in [
        ('normal', unreal.MaterialProperty.MP_NORMAL),
        ('roughness', unreal.MaterialProperty.MP_ROUGHNESS),
        ('metallic', unreal.MaterialProperty.MP_METALLIC),
    ]:
        check(
            f'CONNECT-{mat.get_name()}-{kind}',
            lib.connect_material_property(nodes[kind], 'RGB' if kind == 'normal' else 'R', prop)
        )

    fill = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, -300)
    fill.set_editor_property('r', fill_strength)
    emission = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, -180)
    check(
        'FILL-' + mat.get_name(),
        lib.connect_material_expressions(tinted, '', emission, 'A')
        and lib.connect_material_expressions(fill, '', emission, 'B')
        and lib.connect_material_property(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    )

    lib.recompile_material(mat)
    check('SAVE-' + mat.get_name(), unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False))


try:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    env_source_base = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/SpaceEnvironment_v1'

    def make_fbx_options():
        options = unreal.FbxImportUI()
        for key, value in [
            ('import_mesh', True),
            ('import_as_skeletal', False),
            ('automated_import_should_detect_type', False),
            ('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH),
            ('import_materials', False),
            ('import_textures', False),
        ]:
            options.set_editor_property(key, value)
        settings = options.get_editor_property('static_mesh_import_data')
        for key, value in [
            ('combine_meshes', True),
            ('auto_generate_collision', False),
            ('generate_lightmap_u_vs', False),
            ('convert_scene', True),
            ('convert_scene_unit', True),
        ]:
            settings.set_editor_property(key, value)
        return options

    def import_item(dest, filepath, name, factory, extra=None):
        asset_path = dest + '/' + name
        task = unreal.AssetImportTask()
        for k, v in [
            ('filename', str(filepath)),
            ('destination_path', dest),
            ('destination_name', name),
            ('replace_existing', True),
            ('automated', True),
            ('save', True),
            ('factory', factory),
        ]:
            task.set_editor_property(k, v)
        if extra:
            task.set_editor_property('options', extra)
        tools.import_asset_tasks([task])
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    def load_or_create_mat(dest, name):
        mat_path = dest + '/' + name
        if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
            return unreal.EditorAssetLibrary.load_asset(mat_path)
        return tools.create_asset(name, dest, unreal.Material, unreal.MaterialFactoryNew())

    for ident, fill_val in [('SpaceSkybox', 1.55), ('Asteroid', 1.35), ('BlackHole', 2.40)]:
        dest = '/Game/Presentation/Environment/' + ident
        folder = env_source_base / ident
        mesh = import_item(dest, folder / 'SM_Source.fbx', 'SM_Source', unreal.FbxFactory(), make_fbx_options())
        check('ENV-MESH-' + ident, isinstance(mesh, unreal.StaticMesh))
        tex = {}
        for kind, fname in [('base', 'T_Base.png'), ('normal', 'T_Normal.png'), ('roughness', 'T_Roughness.png'), ('metallic', 'T_Metallic.png')]:
            t = import_item(dest, folder / fname, fname[:-4], unreal.TextureFactory())
            if kind != 'base':
                t.set_editor_property('srgb', False)
                t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if kind == 'normal' else unreal.TextureCompressionSettings.TC_MASKS)
            unreal.EditorAssetLibrary.save_loaded_asset(t, only_if_is_dirty=False)
            tex[kind] = t
        mat = load_or_create_mat(dest, 'M_Source')
        build_readable_material(lib, mat, tex, fill_strength=fill_val, two_sided=True, use_instanced=True)
        mesh.set_material(0, mat)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        report['imported'].append(dest + '/SM_Source')

    # HazardZone (M_Radiation and M_SolarFlare)
    hz_dest = '/Game/Presentation/Environment/HazardZone'
    hz_folder = env_source_base / 'HazardZone'
    hz_mesh = import_item(hz_dest, hz_folder / 'SM_Source.fbx', 'SM_Source', unreal.FbxFactory(), make_fbx_options())
    check('ENV-MESH-HazardZone', isinstance(hz_mesh, unreal.StaticMesh))
    hz_common = {}
    for kind, fname in [('normal', 'T_Normal.png'), ('roughness', 'T_Roughness.png'), ('metallic', 'T_Metallic.png')]:
        t = import_item(hz_dest, hz_folder / fname, fname[:-4], unreal.TextureFactory())
        t.set_editor_property('srgb', False)
        t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if kind == 'normal' else unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(t, only_if_is_dirty=False)
        hz_common[kind] = t
    for mat_name, base_fname, fill_val in [
        ('M_Radiation', 'T_RadiationBase.png', 2.4),
        ('M_SolarFlare', 'T_SolarFlareBase.png', 3.2),
    ]:
        bt = import_item(hz_dest, hz_folder / base_fname, base_fname[:-4], unreal.TextureFactory())
        unreal.EditorAssetLibrary.save_loaded_asset(bt, only_if_is_dirty=False)
        hmat = load_or_create_mat(hz_dest, mat_name)
        build_readable_material(lib, hmat, {'base': bt, **hz_common}, fill_strength=fill_val, two_sided=True, use_instanced=True)
        if mat_name == 'M_Radiation':
            hz_mesh.set_material(0, hmat)
    unreal.EditorAssetLibrary.save_loaded_asset(hz_mesh, only_if_is_dirty=False)
    report['imported'].append(hz_dest + '/SM_Source')

    report['passed'] = True
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    unreal.SystemLibrary.quit_editor()
