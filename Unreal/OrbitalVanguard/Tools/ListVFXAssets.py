import unreal

def inspect_folder(path):
    assets = unreal.EditorAssetLibrary.list_assets(path, recursive=True)
    unreal.log(f"=== Assets in {path} (total {len(assets)}) ===")
    for a in assets:
        data = unreal.EditorAssetLibrary.find_asset_data(a)
        unreal.log(f"  [{data.asset_class_path.asset_name}] {a}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/vfx_starter_assets.txt", "w", encoding="utf-8") as f:
    for path in ["/Game/SpaceshipVFXStarter", "/Game/MixedVFX", "/Game/BlackHole", "/Game/Presentation/Effects"]:
        assets = unreal.EditorAssetLibrary.list_assets(path, recursive=True)
        f.write(f"=== {path} ({len(assets)}) ===\n")
        for a in assets:
            data = unreal.EditorAssetLibrary.find_asset_data(a)
            f.write(f"  [{data.asset_class_path.asset_name}] {a}\n")
unreal.log("Saved assets list")
