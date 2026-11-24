"""신규 원본 두 종의 건설 카드를 반입한다."""
import os
from pathlib import Path
import unreal
root=Path(os.environ.get("OV_BASE_ASSET_ROOT", Path(__file__).resolve().parents[4] / "OrbitalVanguardAssets"))/'Prepared/BaseTrial2'
for name in ('T_YardSmall_Card','T_DockStandard_Card'):
    task=unreal.AssetImportTask()
    task.filename=str(root/(name+'.png'))
    task.destination_name=name
    task.destination_path='/Game/Presentation/Base'
    task.automated=True
    task.save=True
    task.replace_existing=True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assert isinstance(unreal.EditorAssetLibrary.load_asset(task.destination_path+'/'+name),unreal.Texture2D)
