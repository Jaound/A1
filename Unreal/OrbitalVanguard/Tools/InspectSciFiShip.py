import unreal

lines = []
mesh = unreal.EditorAssetLibrary.load_asset("/Game/SpaceshipVFXStarter/Demo/DemoMeshes/SM_SciFiShip.SM_SciFiShip")
if mesh:
    bounds = mesh.get_bounds()
    lines.append("SM_SciFiShip:")
    lines.append(f"  Origin: {bounds.origin}")
    lines.append(f"  BoxExtent: {bounds.box_extent}")
    for m in mesh.get_editor_property("static_materials"):
        lines.append(f"  Mat: {m.material_interface.get_name() if m.material_interface else 'None'}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/scifi_ship_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
