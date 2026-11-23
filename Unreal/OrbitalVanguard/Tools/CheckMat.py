import unreal

mi = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/MI_Planet")
print("MI_Planet:", mi)
if mi:
    print("Is valid:", mi.is_valid_low_level())
    # Let's inspect base material
    parent = mi.get_editor_property("parent")
    print("Parent:", parent)
    if parent:
        print("Parent is valid:", parent.is_valid_low_level())
        # Force compile or check
        unreal.MaterialEditingLibrary.update_material_instance(mi)
        print("Updated MI_Planet")

# Also check legacy M_Planet
m_legacy = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Environment/SpaceSkybox/M_Planet")
print("Legacy M_Planet:", m_legacy)

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/mat_check.txt", "w") as f:
    f.write(f"MI: {mi}\nLegacy: {m_legacy}\n")
