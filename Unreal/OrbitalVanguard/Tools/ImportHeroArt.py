"""프로젝트 기준 경로에서 원화와 비파괴 가공본을 UI 텍스처로 반입한다."""
import json
from pathlib import Path
import unreal as u

root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parents[1]
def import_texture(path, folder, name):
    task=u.AssetImportTask()
    task.filename=str(path)
    task.destination_path=folder
    task.destination_name=name
    task.automated=True
    task.replace_existing=True
    task.save=True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex=u.EditorAssetLibrary.load_asset(folder+"/"+name)
    assert isinstance(tex,u.Texture2D),path
    tex.set_editor_property("compression_settings",u.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group",u.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("address_x",u.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y",u.TextureAddress.TA_CLAMP)
    u.EditorAssetLibrary.save_loaded_asset(tex)
for manifest in sorted((root/"reference/HeroArt").glob("*/Art.json")):
    art=json.loads(manifest.read_text("utf-8"))
    for label,file in art["textures"].items():
        import_texture(manifest.parent/file,"/Game/UI/Heroes/"+art["id"],"T_"+label)
