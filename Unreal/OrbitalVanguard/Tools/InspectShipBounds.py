import unreal

# We can inspect static mesh vertices via unreal.StaticMeshDescription or EditorStaticMeshLibrary
mesh = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Ships/DreadnoughtFull/SM_Hull")
if mesh:
    # Check bounding box min and max
    bounds = mesh.get_bounds()
    print(f"Origin: {bounds.origin}, BoxExtent: {bounds.box_extent}")
    min_x = bounds.origin.x - bounds.box_extent.x
    max_x = bounds.origin.x + bounds.box_extent.x
    print(f"X range: [{min_x}, {max_x}]")

# Also check other ships: Battleship, Carrier, Fighter
for ship_name in ["BattleshipFull", "BattlecarrierFull", "CarrierFull", "FighterFull"]:
    m = unreal.EditorAssetLibrary.load_asset(f"/Game/Presentation/Ships/{ship_name}/SM_Hull")
    if m:
        b = m.get_bounds()
        print(f"{ship_name}: origin={b.origin}, extent={b.box_extent}")
