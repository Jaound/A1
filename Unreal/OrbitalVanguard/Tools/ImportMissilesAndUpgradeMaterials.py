"""미사일 2종·대함미사일모듈·함포탄환·레이저빔·피격이펙트 3D 모델/텍스처 반입 및 모든 무장 M_Source 텍스처 복원."""
from pathlib import Path
import json
import traceback
import unreal

report_path = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation' / 'ImportMissilesAndUpgradeMaterials.json'
report = {
    'passed': False,
    'instanced_flag_prop': None,
    'imported': [],
    'upgraded_materials': [],
    'effects': [],
    'checks': [],
    'errors': [],
}


def check(name, value):
    report['checks'].append({'id': name, 'passed': bool(value)})
    if not value:
        raise RuntimeError('CHECK FAILED: ' + name)


def enable_instanced_usage(mat):
    for prop in ('b_used_with_instanced_static_meshes', 'used_with_instanced_static_meshes'):
        try:
            mat.set_editor_property(prop, True)
            if mat.get_editor_property(prop):
                report['instanced_flag_prop'] = prop
                return True
        except Exception:
            pass
    return False


def build_readable_material(lib, mat, textures, fill_strength=1.35, two_sided=False, use_instanced=True):
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
    source_base = Path.home() / 'Documents/OrbitalVanguardAssets/Source'
    fx_source_base = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/CombatEffects_v1'

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

    def import_item(dest, filepath, name, factory, extra=None, force_reimport=False):
        asset_path = dest + '/' + name
        if not force_reimport and unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            return unreal.EditorAssetLibrary.load_asset(asset_path)
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

    # 1. Import/update Missiles & Anti-Ship Module
    new_models = [
        (
            'StandardMissile',
            source_base / '일반 미사일/Meshy_AI_scifi_standard_missile_20261117_image-to-3d-texture_fbx',
            'Meshy_AI_scifi_standard_missile_20261117_image-to-3d-texture'
        ),
        (
            'AntiShipMissile',
            source_base / '대함 미사일/Meshy_AI_scifi_antiship_heavy_missile_20261117_image-to-3d-texture_fbx',
            'Meshy_AI_scifi_antiship_heavy_missile_20261117_image-to-3d-texture'
        ),
        (
            'AntiShipModule',
            source_base / '대함미사일모듈/Meshy_AI_scifi_antiship_missile_module_20261117_image-to-3d-texture_fbx',
            'Meshy_AI_scifi_antiship_missile_module_20261117_image-to-3d-texture'
        ),
    ]

    for ident, folder, prefix in new_models:
        dest = '/Game/Presentation/Missiles/' + ident
        mesh = import_item(dest, folder / (prefix + '.fbx'), 'SM_Source', unreal.FbxFactory(), make_fbx_options())
        check('MISSILE-MESH-' + ident, isinstance(mesh, unreal.StaticMesh))
        textures = {}
        for kind, suffix, tname in [
            ('base', '.png', 'T_Base'),
            ('normal', '_normal.png', 'T_Normal'),
            ('roughness', '_roughness.png', 'T_Roughness'),
            ('metallic', '_metallic.png', 'T_Metallic'),
        ]:
            tex = import_item(dest, folder / (prefix + suffix), tname, unreal.TextureFactory())
            check(f'MISSILE-TEX-{ident}-{kind}', isinstance(tex, unreal.Texture2D))
            if kind != 'base':
                tex.set_editor_property('srgb', False)
                tex.set_editor_property(
                    'compression_settings',
                    unreal.TextureCompressionSettings.TC_NORMALMAP if kind == 'normal' else unreal.TextureCompressionSettings.TC_MASKS
                )
            unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
            textures[kind] = tex

        mat = load_or_create_mat(dest, 'M_Source')
        build_readable_material(lib, mat, textures, fill_strength=1.40, use_instanced=True)
        mesh.set_material(0, mat)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        report['imported'].append(dest + '/SM_Source')

    # 2. Restore and upgrade all 9 Weapon M_Source materials with verified texture connections
    weapon_ids = [
        'ciws_beam_b', 'ciws_gatling_6', 'heavy_cannon_shc400',
        'laser_turret_lb30', 'missile_pod_mm24', 'missile_pod_mm48',
        'naval_gun_bf120', 'plasma_heavy_cannon', 'railgun_rg200'
    ]
    for wid in weapon_ids:
        wdest = '/Game/Presentation/WeaponsFull/' + wid
        mat = unreal.EditorAssetLibrary.load_asset(wdest + '/M_Source')
        textures = {
            'base': unreal.EditorAssetLibrary.load_asset(wdest + '/T_Base'),
            'normal': unreal.EditorAssetLibrary.load_asset(wdest + '/T_Normal'),
            'roughness': unreal.EditorAssetLibrary.load_asset(wdest + '/T_Roughness'),
            'metallic': unreal.EditorAssetLibrary.load_asset(wdest + '/T_Metallic'),
        }
        check('WEAPON-MAT-' + wid, isinstance(mat, unreal.Material) and all(isinstance(t, unreal.Texture2D) for t in textures.values()))
        build_readable_material(lib, mat, textures, fill_strength=1.45, use_instanced=True)
        report['upgraded_materials'].append(wdest + '/M_Source')

    # 3. Restore and upgrade BF120Trial materials
    for part in ('BaseTrial', 'YawBody', 'PitchTrial'):
        bdest = '/Game/Presentation/BF120Trial'
        mat = unreal.EditorAssetLibrary.load_asset(f'{bdest}/M_BF120_{part}_Trial')
        textures = {
            'base': unreal.EditorAssetLibrary.load_asset(f'{bdest}/T_BF120_{part}_BaseColor_Trial'),
            'normal': unreal.EditorAssetLibrary.load_asset(f'{bdest}/T_BF120_{part}_Normal_Trial'),
            'roughness': unreal.EditorAssetLibrary.load_asset(f'{bdest}/T_BF120_{part}_Roughness_Trial'),
            'metallic': unreal.EditorAssetLibrary.load_asset(f'{bdest}/T_BF120_{part}_Metallic_Trial'),
        }
        if mat and all(textures.values()):
            build_readable_material(lib, mat, textures, fill_strength=1.45, use_instanced=True)
            report['upgraded_materials'].append(f'{bdest}/M_BF120_{part}_Trial')

    # 4. Also enable InstancedStaticMeshes usage flag on FighterFull/M_Readable so fighters never lose their material in game
    fighter_mat = unreal.EditorAssetLibrary.load_asset('/Game/Presentation/Ships/FighterFull/M_Readable')
    if isinstance(fighter_mat, unreal.Material):
        enable_instanced_usage(fighter_mat)
        lib.recompile_material(fighter_mat)
        unreal.EditorAssetLibrary.save_loaded_asset(fighter_mat, only_if_is_dirty=False)
        report['upgraded_materials'].append('/Game/Presentation/Ships/FighterFull/M_Readable')

    # 5. Import 3D CannonShell, LaserBeam, and ImpactBurst models & textures
    # 5a. CannonShell
    cs_dest = '/Game/Presentation/Effects/CannonShell'
    cs_folder = fx_source_base / 'CannonShell'
    cs_mesh = import_item(cs_dest, cs_folder / 'SM_Source.fbx', 'SM_Source', unreal.FbxFactory(), make_fbx_options(), force_reimport=True)
    check('CANNON-SHELL-MESH', isinstance(cs_mesh, unreal.StaticMesh))
    cs_tex = {}
    for kind, fname in [('base', 'T_Base.png'), ('normal', 'T_Normal.png'), ('roughness', 'T_Roughness.png'), ('metallic', 'T_Metallic.png')]:
        t = import_item(cs_dest, cs_folder / fname, fname[:-4], unreal.TextureFactory(), force_reimport=True)
        if kind != 'base':
            t.set_editor_property('srgb', False)
            t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if kind == 'normal' else unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(t, only_if_is_dirty=False)
        cs_tex[kind] = t
    cs_mat = load_or_create_mat(cs_dest, 'M_Source')
    build_readable_material(lib, cs_mat, cs_tex, fill_strength=2.8, two_sided=True, use_instanced=True)
    cs_mesh.set_material(0, cs_mat)
    unreal.EditorAssetLibrary.save_loaded_asset(cs_mesh, only_if_is_dirty=False)
    report['effects'].append(cs_dest + '/SM_Source')

    # 5b. LaserBeam
    lb_dest = '/Game/Presentation/Effects/LaserBeam'
    lb_folder = fx_source_base / 'LaserBeam'
    lb_mesh = import_item(lb_dest, lb_folder / 'SM_Source.fbx', 'SM_Source', unreal.FbxFactory(), make_fbx_options(), force_reimport=True)
    check('LASER-BEAM-MESH', isinstance(lb_mesh, unreal.StaticMesh))
    lb_common = {}
    for kind, fname in [('normal', 'T_Normal.png'), ('roughness', 'T_Roughness.png'), ('metallic', 'T_Metallic.png')]:
        t = import_item(lb_dest, lb_folder / fname, fname[:-4], unreal.TextureFactory(), force_reimport=True)
        t.set_editor_property('srgb', False)
        t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if kind == 'normal' else unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(t, only_if_is_dirty=False)
        lb_common[kind] = t
    for mat_name, base_fname in [('M_Friendly', 'T_FriendlyBase.png'), ('M_Enemy', 'T_EnemyBase.png')]:
        bt = import_item(lb_dest, lb_folder / base_fname, base_fname[:-4], unreal.TextureFactory(), force_reimport=True)
        unreal.EditorAssetLibrary.save_loaded_asset(bt, only_if_is_dirty=False)
        lmat = load_or_create_mat(lb_dest, mat_name)
        build_readable_material(lib, lmat, {'base': bt, **lb_common}, fill_strength=3.4, two_sided=True, use_instanced=True)
        if mat_name == 'M_Friendly':
            lb_mesh.set_material(0, lmat)
    unreal.EditorAssetLibrary.save_loaded_asset(lb_mesh, only_if_is_dirty=False)
    report['effects'].append(lb_dest + '/SM_Source')

    # 5c. ImpactBurst (ShieldImpact, HullImpact, MissilePlume)
    ib_dest = '/Game/Presentation/Effects/ImpactBurst'
    ib_folder = fx_source_base / 'ImpactBurst'
    ib_mesh = import_item(ib_dest, ib_folder / 'SM_Source.fbx', 'SM_Source', unreal.FbxFactory(), make_fbx_options(), force_reimport=True)
    check('IMPACT-BURST-MESH', isinstance(ib_mesh, unreal.StaticMesh))
    ib_common = {}
    for kind, fname in [('normal', 'T_Normal.png'), ('roughness', 'T_Roughness.png'), ('metallic', 'T_Metallic.png')]:
        t = import_item(ib_dest, ib_folder / fname, fname[:-4], unreal.TextureFactory(), force_reimport=True)
        t.set_editor_property('srgb', False)
        t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if kind == 'normal' else unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(t, only_if_is_dirty=False)
        ib_common[kind] = t
    for mat_name, base_fname in [
        ('M_ShieldImpact', 'T_ShieldBase.png'),
        ('M_HullImpact', 'T_HullBase.png'),
        ('M_Plume', 'T_PlumeBase.png'),
    ]:
        bt = import_item(ib_dest, ib_folder / base_fname, base_fname[:-4], unreal.TextureFactory(), force_reimport=True)
        unreal.EditorAssetLibrary.save_loaded_asset(bt, only_if_is_dirty=False)
        imat = load_or_create_mat(ib_dest, mat_name)
        build_readable_material(lib, imat, {'base': bt, **ib_common}, fill_strength=3.2, two_sided=True, use_instanced=True)
        if mat_name == 'M_HullImpact':
            ib_mesh.set_material(0, imat)
    unreal.EditorAssetLibrary.save_loaded_asset(ib_mesh, only_if_is_dirty=False)
    report['effects'].append(ib_dest + '/SM_Source')

    # 6. Ensure M_ShieldBubble is compiled and saved
    fx_dest = '/Game/Presentation/Effects'
    shield_mat = load_or_create_mat(fx_dest, 'M_ShieldBubble')
    lib.delete_all_material_expressions(shield_mat)
    shield_mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    shield_mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    shield_mat.set_editor_property('two_sided', True)
    enable_instanced_usage(shield_mat)

    sh_col = lib.create_material_expression(shield_mat, unreal.MaterialExpressionVectorParameter, -600, -200)
    sh_col.set_editor_property('parameter_name', 'ShieldColor')
    sh_col.set_editor_property('default_value', unreal.LinearColor(0.22, 0.72, 1.0, 1.0))

    sh_alpha = lib.create_material_expression(shield_mat, unreal.MaterialExpressionScalarParameter, -600, 100)
    sh_alpha.set_editor_property('parameter_name', 'ShieldAlpha')
    sh_alpha.set_editor_property('default_value', 0.36)

    fresnel = lib.create_material_expression(shield_mat, unreal.MaterialExpressionFresnel, -600, 300)
    fresnel.set_editor_property('exponent', 2.8)
    fresnel.set_editor_property('base_reflect_fraction', 0.14)

    c_base = lib.create_material_expression(shield_mat, unreal.MaterialExpressionConstant, -400, 220)
    c_base.set_editor_property('r', 0.45)
    c_rim = lib.create_material_expression(shield_mat, unreal.MaterialExpressionConstant, -400, 360)
    c_rim.set_editor_property('r', 1.6)
    rim_mul = lib.create_material_expression(shield_mat, unreal.MaterialExpressionMultiply, -250, 320)
    lib.connect_material_expressions(fresnel, '', rim_mul, 'A')
    lib.connect_material_expressions(c_rim, '', rim_mul, 'B')
    em_factor = lib.create_material_expression(shield_mat, unreal.MaterialExpressionAdd, -120, 260)
    lib.connect_material_expressions(c_base, '', em_factor, 'A')
    lib.connect_material_expressions(rim_mul, '', em_factor, 'B')
    em_col = lib.create_material_expression(shield_mat, unreal.MaterialExpressionMultiply, 20, -80)
    lib.connect_material_expressions(sh_col, 'RGB', em_col, 'A')
    lib.connect_material_expressions(em_factor, '', em_col, 'B')
    lib.connect_material_property(em_col, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    op_mul = lib.create_material_expression(shield_mat, unreal.MaterialExpressionMultiply, -120, 100)
    lib.connect_material_expressions(fresnel, '', op_mul, 'A')
    lib.connect_material_expressions(sh_alpha, '', op_mul, 'B')
    lib.connect_material_property(op_mul, '', unreal.MaterialProperty.MP_OPACITY)

    lib.recompile_material(shield_mat)
    unreal.EditorAssetLibrary.save_loaded_asset(shield_mat, only_if_is_dirty=False)
    report['effects'].append(fx_dest + '/M_ShieldBubble')

    report['passed'] = True
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    unreal.SystemLibrary.quit_editor()
