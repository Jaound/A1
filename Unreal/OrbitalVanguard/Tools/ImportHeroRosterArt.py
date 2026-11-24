"""PrepareHeroRosterArt.py의 검증된 결과를 영웅 ID별 UE 텍스처로 반입한다."""
from pathlib import Path
import json
import unreal as u

project = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
source = project / "Saved/HeroArt34/Prepared"
verified = json.loads((source / "Verified.json").read_text("utf-8"))
for key in verified["prepared"]:
    for kind in ("standing", "portrait", "sd"):
        task = u.AssetImportTask()
        task.filename = str(source/key/(kind+".png"))
        task.destination_path = "/Game/UI/Heroes/"+key
        task.destination_name = "T_"+kind
        task.automated = True
        task.replace_existing = True
        task.save = False
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        path = f"/Game/UI/Heroes/{key}/T_{kind}"
        assert any(str(item).split(".")[0]==path for item in task.imported_object_paths), (key, kind, task.imported_object_paths)
        texture = u.EditorAssetLibrary.load_asset(path)
        assert isinstance(texture,u.Texture2D),path
        texture.set_editor_property("compression_settings",u.TextureCompressionSettings.TC_BC7 if kind=="standing" else u.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("lod_group",u.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("address_x",u.TextureAddress.TA_CLAMP)
        texture.set_editor_property("address_y",u.TextureAddress.TA_CLAMP)
        assert u.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False),path
u.log("영웅 원화 반입 완료: "+str(len(verified["prepared"])*3))
