import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/MI_Planet")
lines = []
if mat:
    vec_params = unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)
    tex_params = unreal.MaterialEditingLibrary.get_texture_parameter_names(mat)
    lines.append(f"Vector params: {vec_params}")
    lines.append(f"Texture params: {tex_params}")
    for name in vec_params:
        val = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(mat, name)
        lines.append(f"  Vec {name}: {val}")
    for name in tex_params:
        val = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mat, name)
        lines.append(f"  Tex {name}: {val.get_name() if val else 'None'}")
else:
    lines.append("MI_Planet not found")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/mi_planet_full.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
