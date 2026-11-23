import unreal

mesh = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Static_Meshes/SM_Earth_Planet")
lines = []
if mesh:
    lines.append(f"Mesh: {mesh.get_name()}")
    num_mats = mesh.get_num_sections(0) if hasattr(mesh, "get_num_sections") else "unknown"
    lines.append(f"Num sections: {num_mats}")
    # static materials
    for i, sm in enumerate(mesh.static_materials):
        lines.append(f"Slot {i}: name={sm.material_slot_name}, mat={sm.material_interface.get_path_name() if sm.material_interface else 'None'}")
else:
    lines.append("SM_Earth_Planet not found")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/mesh_slots_out.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
