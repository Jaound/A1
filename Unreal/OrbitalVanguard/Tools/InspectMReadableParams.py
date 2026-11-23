import unreal

mat = unreal.EditorAssetLibrary.load_asset('/Game/Presentation/Ships/DreadnoughtFull/M_Readable.M_Readable')
lines = []
if mat:
    lines.append(f"Mat: {mat.get_name()}")
    s_names = unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)
    v_names = unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)
    t_names = unreal.MaterialEditingLibrary.get_texture_parameter_names(mat)
    lines.append(f"Scalars: {[str(x) for x in s_names]}")
    lines.append(f"Vectors: {[str(x) for x in v_names]}")
    lines.append(f"Textures: {[str(x) for x in t_names]}")
    for s in s_names:
        lines.append(f"  Scalar {s} = {unreal.MaterialEditingLibrary.get_material_default_scalar_parameter_value(mat, s)}")
    for v in v_names:
        val = unreal.MaterialEditingLibrary.get_material_default_vector_parameter_value(mat, v)
        lines.append(f"  Vector {v} = ({val.r:.2f}, {val.g:.2f}, {val.b:.2f}, {val.a:.2f})")

with open('C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/m_readable_params_full.txt', 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
