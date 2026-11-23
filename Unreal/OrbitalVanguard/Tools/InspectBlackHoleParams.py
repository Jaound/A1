import unreal

out = []
def inspect_mat(path):
    mat = unreal.EditorAssetLibrary.load_asset(path)
    out.append(f"=== {path} ===")
    if mat:
        for s in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat):
            out.append(f"  Scalar: {s}")
        for v in unreal.MaterialEditingLibrary.get_vector_parameter_names(mat):
            out.append(f"  Vector: {v}")

inspect_mat("/Game/BlackHole/Materials/M_Blackhole.M_Blackhole")
inspect_mat("/Game/BlackHole/Materials/MI_Blackhole_Physical.MI_Blackhole_Physical")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/blackhole_params.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
