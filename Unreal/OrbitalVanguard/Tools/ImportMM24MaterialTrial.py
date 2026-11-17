"""Import experimental baked maps/materials for the isolated MM24 UE meshes.

Only new trial assets are created. The meshes are not wired into the game.
Source image files are checked before and after import; no asset is replaced.
"""
import hashlib
import json
import os
from pathlib import Path
import traceback
import unreal

base=Path.home()/'Documents/OrbitalVanguardAssets/Prepared'
prefix=os.environ.get('OV_TURRET_PREFIX','MM24')
if not prefix.replace('_','').isalnum():raise ValueError('Invalid part prefix')
color=base/os.environ.get('OV_TURRET_COLOR_FOLDER','PlayUI_R2_MM24_BakeTrial_v4')
maps=base/os.environ.get('OV_TURRET_MATERIAL_FOLDER','PlayUI_R2_MM24_MaterialTrial_v1')
destination='/Game/Presentation/'+prefix+'Trial'
output=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/('Validation/'+prefix+'UEMaterials.json')
report={'passed':False,'checks':[],'assets':[],'errors':[],
        'scope':'isolated baked trial materials, not final art or gameplay'}
def check(name,value,detail=''):
    report['checks'].append({'id':name,'passed':bool(value),'detail':str(detail)})
    if not value:raise AssertionError(name+': '+str(detail))
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
try:
    entries=[]
    for part in ('Base','Yaw'):
        part_name=prefix+'_'+part+'Trial'
        for kind,path in [('BaseColor',color/(part_name+'_baked.png')),('Normal',maps/(part_name+'_NORMAL.png')),
                          ('Roughness',maps/(part_name+'_Roughness.png')),('Metallic',maps/(part_name+'_Metallic.png'))]:
            entries.append((part,kind,path,'T_'+prefix+'_'+part+'_'+kind+'_Trial'))
        check('EXISTING-TRIAL-MESH-'+part,isinstance(unreal.EditorAssetLibrary.load_asset(destination+'/SM_'+part_name),unreal.StaticMesh))
        check('NO-MATERIAL-OVERWRITE-'+part,not unreal.EditorAssetLibrary.does_asset_exist(destination+'/M_'+prefix+'_'+part+'_Trial'))
    for part,kind,path,asset in entries:
        check('SOURCE-'+asset,path.is_file());check('NO-OVERWRITE-'+asset,not unreal.EditorAssetLibrary.does_asset_exist(destination+'/'+asset))
    hashes={str(path):sha(path) for _,_,path,_ in entries};tools=unreal.AssetToolsHelpers.get_asset_tools();textures={}
    for part,kind,path,asset in entries:
        task=unreal.AssetImportTask();task.set_editor_property('filename',str(path));task.set_editor_property('destination_path',destination)
        task.set_editor_property('destination_name',asset);task.set_editor_property('automated',True)
        task.set_editor_property('replace_existing',False);task.set_editor_property('save',True);task.set_editor_property('factory',unreal.TextureFactory())
        tools.import_asset_tasks([task]);texture=unreal.EditorAssetLibrary.load_asset(destination+'/'+asset)
        check('TEXTURE-'+asset,isinstance(texture,unreal.Texture2D))
        if kind!='BaseColor':
            texture.set_editor_property('srgb',False)
            texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if kind=='Normal' else unreal.TextureCompressionSettings.TC_MASKS)
        if kind=='Normal':texture.set_editor_property('flip_green_channel',True)
        check('SAVE-'+asset,unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False));textures[(part,kind)]=texture
        report['assets'].append(destination+'/'+asset)
    library=unreal.MaterialEditingLibrary
    for part in ('Base','Yaw'):
        name='M_'+prefix+'_'+part+'_Trial';mat=tools.create_asset(name,destination,unreal.Material,unreal.MaterialFactoryNew())
        check('CREATE-'+name,isinstance(mat,unreal.Material))
        for index,(kind,prop) in enumerate([('BaseColor',unreal.MaterialProperty.MP_BASE_COLOR),('Normal',unreal.MaterialProperty.MP_NORMAL),
                                              ('Roughness',unreal.MaterialProperty.MP_ROUGHNESS),('Metallic',unreal.MaterialProperty.MP_METALLIC)]):
            node=library.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-500,index*220)
            node.set_editor_property('texture',textures[(part,kind)])
            node.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='Normal' else
                                     unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if kind=='BaseColor' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            check('CONNECT-'+part+'-'+kind,library.connect_material_property(node,'RGB' if kind in ('BaseColor','Normal') else 'R',prop))
        library.recompile_material(mat);check('SAVE-'+name,unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False))
        mesh=unreal.EditorAssetLibrary.load_asset(destination+'/SM_'+prefix+'_'+part+'Trial')
        mesh.set_material(0,mat);check('SAVE-TRIAL-MESH-'+part,unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False))
        report['assets'].append(destination+'/'+name)
    check('SOURCE-IMAGES-UNCHANGED',all(sha(Path(p))==digest for p,digest in hashes.items()))
    report['source_sha256']=hashes;report['passed']=True
except Exception:
    report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1])
finally:
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    unreal.log('OV_MM24_UE_MATERIAL '+str(report['passed']));unreal.SystemLibrary.quit_editor()
