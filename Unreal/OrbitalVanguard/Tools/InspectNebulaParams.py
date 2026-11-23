import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/StarfieldFree/Materials/M_Starfield_Nebula_Free_v02")
lines = []
if mat:
    s_params = unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)
    v_params = unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)
    t_params = unreal.MaterialEditingLibrary.get_texture_parameter_names(mat)
    lines.append("=== M_Starfield_Nebula_Free_v02 ===")
    lines.append(f"Scalars: {[str(p) for p in s_params]}")
    lines.append(f"Vectors: {[str(p) for p in v_params]}")
    lines.append(f"Textures: {[str(p) for p in t_params]}")

planet = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/MI_Planet")
if planet:
    s_params = unreal.MaterialEditingLibrary.get_scalar_parameter_names(planet)
    v_params = unreal.MaterialEditingLibrary.get_vector_parameter_names(planet)
    lines.append("=== MI_Planet ===")
    lines.append(f"Scalars: {[str(p) for p in s_params]}")
    lines.append(f"Vectors: {[str(p) for p in v_params]}")
    for p in s_params:
        v = unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(planet, p)
        lines.append(f"  Scalar {p} = {v}")
    for p in v_params:
        v = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(planet, p)
        lines.append(f"  Vector {p} = ({v.r:.2f}, {v.g:.2f}, {v.b:.2f}, {v.a:.2f})")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/nebula_mat_params.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
