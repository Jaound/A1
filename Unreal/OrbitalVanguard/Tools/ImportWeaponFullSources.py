"""원본 기반 무장 9종을 새 UE 정적 메시·재질로 반입한다.

기존 에셋을 덮어쓰지 않는다. 단일 메시의 포탑 가동/장착은 별도 검증이 필요하다.
"""
import hashlib
import json
import os
from pathlib import Path
import traceback
import unreal

source_root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponFullSources_v1'
models = json.loads((source_root/'report.json').read_text(encoding='utf8'))['models']
selected = os.environ.get('OV_WEAPON_SOURCE_ID','')
if selected:
    models=[model for model in models if model['id']==selected]
output = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'Validation'/('WeaponFullSourcesImport_'+(selected or 'all')+'.json')
if output.exists():
    raise FileExistsError('기존 반입 보고서를 덮어쓰지 않음: '+str(output))
report = {'scope':'원본 전체 메시·텍스처 UE 격리 반입. 가동부·장착·포구·실전 미검증.', 'passed':False, 'models':[], 'errors':[]}

def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def check(label, condition):
    if not condition:
        raise RuntimeError(label)

def save_report():
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')

try:
    check('EXPECTED SOURCE MODELS',len(models)==(1 if selected else 9))
    for entry in models:
        dest='/Game/Presentation/WeaponsFull/'+entry['id']
        check('NO EXISTING DESTINATION '+dest,not unreal.EditorAssetLibrary.does_directory_exist(dest))
        check('SOURCE UNCHANGED '+entry['id'],sha(entry['source_fbx'])==entry['source_sha256'] and
              sha(entry['prepared_fbx'])==entry['prepared_sha256'])
        for tex in entry['textures']:
            check('TEXTURE HASH '+tex['file'],sha(source_root/entry['id']/tex['file'])==tex['sha256'])
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    library=unreal.MaterialEditingLibrary
    for entry in models:
        ident=entry['id'];dest='/Game/Presentation/WeaponsFull/'+ident
        options=unreal.FbxImportUI()
        for key,value in [('import_mesh',True),('import_as_skeletal',False),('automated_import_should_detect_type',False),
                          ('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH),('import_materials',False),('import_textures',False)]:
            options.set_editor_property(key,value)
        settings=options.get_editor_property('static_mesh_import_data')
        for key,value in [('combine_meshes',True),('auto_generate_collision',False),('generate_lightmap_u_vs',False),
                          ('convert_scene',True),('convert_scene_unit',True)]:settings.set_editor_property(key,value)
        def task(path,name,factory,extra=None):
            item=unreal.AssetImportTask()
            for key,value in [('filename',str(path)),('destination_path',dest),('destination_name',name),
                              ('replace_existing',False),('automated',True),('save',True),('factory',factory)]:item.set_editor_property(key,value)
            if extra:item.set_editor_property('options',extra)
            tools.import_asset_tasks([item])
            return unreal.EditorAssetLibrary.load_asset(dest+'/'+name)
        mesh=task(entry['prepared_fbx'],'SM_Source',unreal.FbxFactory(),options)
        check('STATIC MESH '+ident,isinstance(mesh,unreal.StaticMesh))
        triangles=mesh.get_num_triangles(0)
        check('SOURCE TRIANGLES '+ident,triangles==sum(m['polygons'] for m in entry['meshes']))
        check('UV '+ident,editor.get_num_uv_channels(mesh,0)>0)
        check('NO COLLISION '+ident,editor.get_simple_collision_count(mesh)==0)
        textures={}
        for kind,filename in [('base','Image_0.jpg'),('normal','Image_2.jpg'),
                              ('metallic','texture_0_metallic.png.png'),('roughness','texture_0_roughness.png.png')]:
            texture=task(source_root/ident/filename,'T_'+kind.title(),unreal.TextureFactory())
            check('TEXTURE '+ident+' '+kind,isinstance(texture,unreal.Texture2D))
            if kind!='base':
                texture.set_editor_property('srgb',False)
                texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if kind=='normal' else unreal.TextureCompressionSettings.TC_MASKS)
            check('TEXTURE SAVE '+ident+' '+kind,unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False))
            textures[kind]=texture
        mat=tools.create_asset('M_Source',dest,unreal.Material,unreal.MaterialFactoryNew())
        check('MATERIAL '+ident,isinstance(mat,unreal.Material))
        props={'base':unreal.MaterialProperty.MP_BASE_COLOR,'normal':unreal.MaterialProperty.MP_NORMAL,
               'metallic':unreal.MaterialProperty.MP_METALLIC,'roughness':unreal.MaterialProperty.MP_ROUGHNESS}
        for index,kind in enumerate(('base','normal','metallic','roughness')):
            node=library.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-500,index*220)
            node.set_editor_property('texture',textures[kind])
            node.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='normal' else
                unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if kind=='base' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            check('MATERIAL LINK '+ident+' '+kind,library.connect_material_property(node,'RGB' if kind in ('base','normal') else 'R',props[kind]))
        library.recompile_material(mat)
        check('MATERIAL SAVE '+ident,unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False))
        mesh.set_material(0,mat)
        check('MESH SAVE '+ident,unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False))
        report['models'].append({'id':ident,'mesh':dest+'/SM_Source','material':dest+'/M_Source','triangles':triangles,
                                 'source_sha256':entry['source_sha256'],'articulated':False,'battle_applied':False})
        save_report();unreal.log('OV_WEAPON_FULL_IMPORTED '+ident)
    report['passed']=len(report['models'])==len(models)
except Exception:
    report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1])
finally:
    save_report();unreal.SystemLibrary.quit_editor()
