"""Restore ship normals directly from the artist's external PNG exports.

Set OV_HULL_SOURCE to OrbitalVanguardAssets/Source and execute this script with
UnrealEditor-Cmd -run=pythonscript. The full-resolution corvette already uses
a lossless normal. Material references, UVs, texture paths and settings stay
with the existing assets. The FBX embedded previews must not be used for this.
"""
import hashlib
import json
import os
import struct
from pathlib import Path

import unreal as ue


SHIP_FOLDERS = {
    "Air_Defense": "방공함", "Assault": "강습선", "Bastion": "바스티온급 구축함",
    "Battlecarrier": "항공순양함", "Battleship": "전함", "Carrier": "항공모함",
    "Cruiser": "순양함", "Dreadnought": "구축전함", "Fighter": "함재기",
    "Frigate": "호위함", "Mining": "채굴선", "Missile_Ship": "전략미사일함",
    "Radar": "레이더 함", "Repair": "보급수리선", "Transport": "수송선",
    "Vector": "백터급 구축함",
}
PRESERVE = ("compression_settings", "srgb", "flip_green_channel", "lod_group",
            "lod_bias", "max_texture_size", "never_stream", "mip_gen_settings", "address_x", "address_y")


def source_png(source, folder):
    found = list((source / folder).rglob("*_normal.png"))
    if len(found) != 1:
        raise RuntimeError(f"{folder}: expected one source normal PNG, found {len(found)}")
    return found[0]


def restore_all():
    source = Path(os.environ["OV_HULL_SOURCE"]).resolve(strict=True)
    # Resolve every input before replacing any texture.
    inputs = {key: source_png(source, folder) for key, folder in SHIP_FOLDERS.items()}
    tools = ue.AssetToolsHelpers.get_asset_tools()
    saved = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_saved_dir()))
    report_path = saved / "Validation/HullSourceNormals.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    output = {}
    for hull, filename in inputs.items():
        folder = f"/Game/Presentation/Ships/{hull}Full"
        asset = folder + "/T_Normal"
        old = ue.load_asset(asset)
        if not isinstance(old, ue.Texture2D):
            raise RuntimeError(f"Missing existing texture {asset}")
        settings = {key: old.get_editor_property(key) for key in PRESERVE}
        if settings["compression_settings"] != ue.TextureCompressionSettings.TC_NORMALMAP or settings["srgb"]:
            raise RuntimeError(f"{asset}: unexpected tangent normal settings")
        with filename.open("rb") as handle:
            header = handle.read(24)
            if header[:8] != b"\x89PNG\r\n\x1a\n":
                raise RuntimeError(f"Invalid PNG: {filename}")
            width, height = struct.unpack(">II", header[16:24])
            handle.seek(0)
            source_sha = hashlib.file_digest(handle, "sha256").hexdigest()
        task = ue.AssetImportTask()
        task.set_editor_property("filename", str(filename))
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", "T_Normal")
        task.set_editor_property("factory", ue.TextureFactory())
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("replace_existing_settings", False)
        task.set_editor_property("automated", True)
        tools.import_asset_tasks([task])
        if asset + ".T_Normal" not in list(task.imported_object_paths):
            raise RuntimeError(f"Import did not replace {asset}: {task.imported_object_paths}")
        tex = ue.load_asset(asset)
        for key, value in settings.items():
            if tex.get_editor_property(key) != value:
                tex.set_editor_property(key, value)
        if not ue.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {asset}")
        imported = tex.get_editor_property("asset_import_data").extract_filenames()
        if not any(Path(name).resolve() == filename for name in imported):
            raise RuntimeError(f"{asset}: wrong import source {imported}")
        output[hull.lower()] = {"asset": asset, "source": filename.relative_to(source).as_posix(),
                               "sha256": source_sha, "size": [width,height],
                               "flip_green": settings["flip_green_channel"], "sRGB": settings["srgb"],
                               "compression": "BC5 normal map"}
        ue.log(f"OV_SOURCE_NORMAL {hull}: {width}x{height}")
    report_path.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")


restore_all()
