"""신규 원본 두 종의 건설 카드를 반입한다."""
from pathlib import Path
import unreal
root=Path.home()/'Documents/OrbitalVanguardAssets/Prepared/BaseTrial2'
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
