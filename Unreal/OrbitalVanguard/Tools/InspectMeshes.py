import unreal

out_lines = []
def inspect_mesh(path):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not mesh:
        out_lines.append(f"Cannot load {path}")
        return
    lod_count = unreal.EditorStaticMeshLibrary.get_lod_count(mesh)
    materials = mesh.get_editor_property("static_materials")
    out_lines.append(f"Mesh: {path}")
    out_lines.append(f"  LOD count: {lod_count}")
    for i, mat in enumerate(materials):
        mat_iface = mat.material_interface
        out_lines.append(f"  Slot {i}: {mat.material_slot_name} -> {mat_iface.get_name() if mat_iface else 'None'}")

inspect_mesh("/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull")
inspect_mesh("/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull")
inspect_mesh("/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull")
inspect_mesh("/Game/SpaceshipVFXStarter/Demo/DemoMeshes/SM_SciFiShip.SM_SciFiShip")
inspect_mesh("/Game/StarfieldFree/Mesh/SM_Nebula_Sphere_Free.SM_Nebula_Sphere_Free")
inspect_mesh("/Game/StarfieldFree/Mesh/SM_StarSphereFree.SM_StarSphereFree")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/mesh_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out_lines))
