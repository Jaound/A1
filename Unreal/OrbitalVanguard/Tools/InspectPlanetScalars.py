import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/MI_Planet")
lines = []
if mat:
    names = unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)
    for n in names:
        val = unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mat, n)
        lines.append(f"{n} = {val}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/mi_planet_scalars.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
