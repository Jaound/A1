import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Ships/DreadnoughtFull/M_Readable.M_Readable")
lines = []
if mat:
    lines.append("M_Readable loaded")
    for t in unreal.MaterialEditingLibrary.get_texture_parameter_names(mat):
        lines.append(f"Texture param: {t}")
    for s in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat):
        lines.append(f"Scalar param: {s}")
    for v in unreal.MaterialEditingLibrary.get_vector_parameter_names(mat):
        lines.append(f"Vector param: {v}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/m_readable_params.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
