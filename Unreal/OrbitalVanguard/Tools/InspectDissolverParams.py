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

inspect_mat("/Game/MixedVFX/Materials/M_DissolverMaster.M_DissolverMaster")
inspect_mat("/Game/MixedVFX/Materials/M_SimpleDissolver.M_SimpleDissolver")
inspect_mat("/Game/MixedVFX/Materials/M_Slash_Master.M_Slash_Master")
inspect_mat("/Game/MixedVFX/SceneAssets/Materials/M_SceneMaterialMaster.M_SceneMaterialMaster")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/dissolver_params.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
