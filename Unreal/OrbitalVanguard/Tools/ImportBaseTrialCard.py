"""실제 검증 장면에서 준비한 기지 썸네일을 임무 카드로 반입한다."""
from pathlib import Path
import unreal

lib=unreal.EditorAssetLibrary
asset='/Game/UI/MissionCards/BaseTrial'
if lib.does_asset_exist(asset):
    raise RuntimeError('기존 기지 카드가 있습니다.')
source=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'Validation/BaseTrial/Card.png'
task=unreal.AssetImportTask()
task.filename=str(source)
task.destination_path='/Game/UI/MissionCards'
task.destination_name='BaseTrial'
task.factory=unreal.TextureFactory()
task.automated=True
task.save=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assert isinstance(lib.load_asset(asset),unreal.Texture2D)
