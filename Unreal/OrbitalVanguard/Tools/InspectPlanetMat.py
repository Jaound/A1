import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/MI_Planet")
out = []
if mat:
    out.append(f"Material: {mat.get_name()}, Class: {mat.get_class().get_name()}")
    # base material
    base = mat.get_editor_property("parent")
    out.append(f"Parent: {base.get_name() if base else 'None'}")
    
    # check parameters
    scalar_params = unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)
    out.append(f"Scalar params: {scalar_params}")
else:
    out.append("MI_Planet not found")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/mi_planet_inspect.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
