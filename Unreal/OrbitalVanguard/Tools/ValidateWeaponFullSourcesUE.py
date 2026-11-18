"""새 에디터에서 원본 무장 정적 에셋을 다시 열어 구조를 확인한다."""
import json
from pathlib import Path
import traceback
import unreal

root=Path.home()/'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponFullSources_v1'
models=json.loads((root/'report.json').read_text(encoding='utf8'))['models']
output=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'Validation/WeaponFullSourcesUEValidation.json'
if output.exists():raise FileExistsError(output)
result={'scope':'독립 에디터 정적 에셋 재로드. 전장 적용/회전/포구/직접 플레이는 미검증.', 'passed':False,'models':[],'errors':[]}
try:
    editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for model in models:
        dest='/Game/Presentation/WeaponsFull/'+model['id']
        mesh=unreal.EditorAssetLibrary.load_asset(dest+'/SM_Source')
        material=unreal.EditorAssetLibrary.load_asset(dest+'/M_Source')
        textures=[unreal.EditorAssetLibrary.load_asset(dest+'/T_'+k) for k in ('Base','Normal','Metallic','Roughness')]
        ok=isinstance(mesh,unreal.StaticMesh) and isinstance(material,unreal.Material) and all(isinstance(t,unreal.Texture2D) for t in textures)
        if not ok:raise RuntimeError('ASSET LOAD '+model['id'])
        triangles=mesh.get_num_triangles(0);uv=editor.get_num_uv_channels(mesh,0)
        slot=mesh.get_material(0)
        ok=triangles==sum(m['polygons'] for m in model['meshes']) and uv>0 and slot==material
        result['models'].append({'id':model['id'],'triangles':triangles,'uv_channels':uv,'material_matches':slot==material,'passed':ok})
        if not ok:raise RuntimeError('ASSET CONTENT '+model['id'])
        unreal.log('OV_WEAPON_FULL_RELOAD '+model['id']+' '+str(triangles))
    result['passed']=len(result['models'])==9
except Exception:
    result['errors'].append(traceback.format_exc());unreal.log_error(result['errors'][-1])
finally:
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    unreal.SystemLibrary.quit_editor()
