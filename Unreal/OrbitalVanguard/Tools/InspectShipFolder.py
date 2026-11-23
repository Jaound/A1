import unreal

out = []
def check_ship_folder(ship_name):
    folder = f"/Game/Presentation/Ships/{ship_name}"
    assets = unreal.EditorAssetLibrary.list_assets(folder)
    out.append(f"Ship: {ship_name} ({len(assets)} assets)")
    for a in assets:
        cls = unreal.EditorAssetLibrary.find_asset_data(a).asset_class_path.asset_name
        out.append(f"  {a} [{cls}]")

check_ship_folder("DreadnoughtFull")
check_ship_folder("BattlecarrierFull")
check_ship_folder("BattleshipFull")
check_ship_folder("CarrierFull")
check_ship_folder("BastionFull")
check_ship_folder("VectorFull")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/ship_assets_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
