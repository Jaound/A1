"""Import two separate MM24 trial meshes into a new UE-only inspection path.

Does not replace assets or change combat visuals. This does not certify pitch,
materials, source art fidelity, performance or real-game operation.
"""
import hashlib
import json
import os
from pathlib import Path
import traceback
import unreal

prefix=os.environ.get('OV_TURRET_PREFIX','MM24')
if not prefix.replace('_','').isalnum():raise ValueError('Invalid part prefix')
source=Path.home()/'Documents/OrbitalVanguardAssets/Prepared'/('PlayUI_R2_'+prefix+'_UEExport_v1')
destination='/Game/Presentation/'+prefix+'Trial'
output=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/('Validation/'+prefix+'UETrial.json')
report={'passed':False,'checks':[],'parts':[],'errors':[],
        'scope':'isolated UE mesh inspection; not integrated into gameplay or final art'}
def check(name,value,detail=''):
    report['checks'].append({'id':name,'passed':bool(value),'detail':str(detail)})
    if not value:raise AssertionError(name+': '+str(detail))
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
try:
    source_report=json.loads((source/'report.json').read_text(encoding='utf8'))
    check('SOURCE-INPUT-UNCHANGED',source_report['source_before']==source_report['source_after'])
    for part in source_report['parts']:
        path=Path(part['fbx']);asset='SM_'+part['name']
        check('SOURCE-HASH-'+asset,path.is_file() and sha(path)==part['sha256'])
        check('NO-OVERWRITE-'+asset,not unreal.EditorAssetLibrary.does_asset_exist(destination+'/'+asset))
    editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    check('EDITOR',editor is not None)
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    for part in source_report['parts']:
        path=Path(part['fbx']);name='SM_'+part['name'];asset_path=destination+'/'+name
        options=unreal.FbxImportUI();options.set_editor_property('import_mesh',True)
        options.set_editor_property('import_as_skeletal',False);options.set_editor_property('automated_import_should_detect_type',False)
        options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
        options.set_editor_property('import_materials',False);options.set_editor_property('import_textures',False)
        settings=options.get_editor_property('static_mesh_import_data')
        settings.set_editor_property('combine_meshes',True);settings.set_editor_property('auto_generate_collision',False)
        settings.set_editor_property('generate_lightmap_u_vs',False);settings.set_editor_property('convert_scene',True)
        settings.set_editor_property('convert_scene_unit',True);settings.set_editor_property('import_uniform_scale',1.0)
        task=unreal.AssetImportTask();task.set_editor_property('filename',str(path))
        task.set_editor_property('destination_path',destination);task.set_editor_property('destination_name',name)
        task.set_editor_property('replace_existing',False);task.set_editor_property('automated',True)
        task.set_editor_property('save',True);task.set_editor_property('factory',unreal.FbxFactory());task.set_editor_property('options',options)
        tools.import_asset_tasks([task]);mesh=unreal.EditorAssetLibrary.load_asset(asset_path)
        check('STATIC-MESH-'+name,isinstance(mesh,unreal.StaticMesh))
        bounds=mesh.get_bounding_box();size=[float(getattr(bounds.max,k)-getattr(bounds.min,k)) for k in ('x','y','z')]
        item={'asset':asset_path,'size_cm':size,'triangles':mesh.get_num_triangles(0),
              'uv_channels':editor.get_num_uv_channels(mesh,0),'simple_collisions':editor.get_simple_collision_count(mesh)}
        report['parts'].append(item)
        check('TRIANGLES-'+name,abs(item['triangles']-part['faces'])<=10,item)
        check('UV-'+name,item['uv_channels']==1,item)
        check('NO-COLLISION-'+name,item['simple_collisions']==0,item)
        check('SCALE-'+name,all(.1<x<500 for x in size),item)
        check('SAVE-'+name,unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False))
        check('SOURCE-STILL-SAME-'+name,sha(path)==part['sha256'])
    report['passed']=True
except Exception:
    report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1])
finally:
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    unreal.log('OV_MM24_UE_IMPORT '+str(report['passed']));unreal.SystemLibrary.quit_editor()
