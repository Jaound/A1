"""새로 반입한 원본 함재기 LOD0를 유지하고 거리별 시험 LOD를 생성한다."""
import json
from pathlib import Path
import traceback
import unreal

asset='/Game/Presentation/Ships/FighterFull/SM_Hull'
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'Validation/FighterLOD.json'
report={'passed':False,'scope':'LOD0 원본 유지, 시험 거리별 축소 LOD. 실제 대량 성능 별도 검증','errors':[]}
try:
    mesh=unreal.EditorAssetLibrary.load_asset(asset)
    if not isinstance(mesh,unreal.StaticMesh): raise RuntimeError('원본 함재기 메시 없음')
    editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    original=mesh.get_num_triangles(0)
    if original!=755812 or editor.get_lod_count(mesh)!=1: raise RuntimeError('예상한 신규 원본 메시가 아님; 덮어쓰기 중단')
    options=unreal.StaticMeshReductionOptions()
    options.set_editor_property('auto_compute_lod_screen_size',False)
    levels=[]
    for ratio,screen in [(1.,1.),(.05,.12),(.01,.03)]:
        level=unreal.StaticMeshReductionSettings()
        level.set_editor_property('percent_triangles',ratio)
        level.set_editor_property('screen_size',screen)
        levels.append(level)
    options.set_editor_property('reduction_settings',levels)
    result=editor.set_lods(mesh,options)
    if result!=3: raise RuntimeError('LOD 생성 실패: '+str(result))
    counts=[mesh.get_num_triangles(i) for i in range(3)]
    if counts[0]!=original or not (0<counts[2]<counts[1]<counts[0]): raise RuntimeError('LOD 삼각형 검증 실패: '+str(counts))
    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False): raise RuntimeError('저장 실패')
    report.update(passed=True,triangles=counts,screen_sizes=[1.,.12,.03])
except Exception:
    report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1])
finally:
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    unreal.SystemLibrary.quit_editor()
