"""선택한 원본으로 프로젝트 서체를 재생성한다. OV_FONT_SOURCE는 반입할 때만 필요하다."""
import os
from pathlib import Path
from zipfile import ZipFile
import unreal as ue

source = Path(os.environ["OV_FONT_SOURCE"])
scratch = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_saved_dir())) / "Typography"
scratch.mkdir(exist_ok=True)
folder = "/Game/UI/Fonts/Typography"
styles = {
    "Body": ("NaverNanumSquareNeo.zip", "NanumSquareNeo/TTF/NanumSquareNeo-bRg.ttf"),
    "Heading": ("NaverNanumSquareNeo.zip", "NanumSquareNeo/TTF/NanumSquareNeo-dEb.ttf"),
    "Logo": ("orbitron-master.zip", "orbitron-master/Orbitron Bold.ttf"),
    "Numeric": ("D2Coding-Ver1.4.0-20261003.zip", "D2Coding/D2Coding-Ver1.4.0-20261003.ttf"),
    "Comms": (None, "Orbit-Regular.ttf"),
}
tools = ue.AssetToolsHelpers.get_asset_tools()
assets = ue.EditorAssetLibrary
for role, (archive, member) in styles.items():
    path = source / member
    if archive:
        with ZipFile(source / archive) as files:
            path = scratch / Path(member).name
            path.write_bytes(files.read(member))
    task = ue.AssetImportTask()
    factory = ue.FontFileImportFactory()
    factory.batch_create_font_asset = ue.BatchCreateFontAsset.NO
    task.factory = factory
    task.filename = str(path)
    task.destination_path = folder
    task.destination_name = "Face_" + role
    task.automated = task.replace_existing = task.save = True
    tools.import_asset_tasks([task])
    face = assets.load_asset(f"{folder}/Face_{role}")
    assert isinstance(face, ue.FontFace), role

def typeface(role):
    face = f"{folder}/Face_{role}.Face_{role}"
    return f'''(Fonts=((Name="Regular",Font=(FontFaceAsset="/Script/Engine.FontFace'{face}'"))))'''

for role in styles:
    name = "Font_" + role
    path = f"{folder}/{name}"
    font = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name, folder, ue.Font, ue.FontFactory())
    font.set_editor_property("font_cache_type", ue.FontCacheType.RUNTIME)
    font.set_editor_property("legacy_font_name", "Regular")
    font.set_editor_property("legacy_font_size", 10)
    composite = font.get_editor_property("composite_font")
    composite.import_text(f"(DefaultTypeface={typeface(role)},FallbackTypeface=(Typeface={typeface('Body')}))")
    assert f"Face_{role}.Face_{role}" in composite.export_text(), composite.export_text()
    font.set_editor_property("composite_font", composite)
    assert assets.save_loaded_asset(font, only_if_is_dirty=False)
    ue.log("OV typography " + role + ": " + font.get_editor_property("composite_font").export_text())
