import unreal

ships = [
    "DreadnoughtFull",
    "BattleshipFull",
    "BattlecarrierFull",
    "CarrierFull",
    "CruiserFull",
    "BastionFull",
    "VectorFull",
    "Air_DefenseFull",
    "FrigateFull",
    "FighterFull"
]

lines = []
for s in ships:
    mesh = unreal.EditorAssetLibrary.load_asset(f"/Game/Presentation/Ships/{s}/SM_Hull")
    if mesh:
        b = mesh.get_bounds()
        lines.append(f"{s}: origin=({b.origin.x:.1f}, {b.origin.y:.1f}, {b.origin.z:.1f}), extent=({b.box_extent.x:.1f}, {b.box_extent.y:.1f}, {b.box_extent.z:.1f})")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/ship_axes.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
