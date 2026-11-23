import unreal

mat_path = "/Game/Planet_Project/Assets/Materials/MI_Planet.MI_Planet"
mat = unreal.EditorAssetLibrary.load_asset(mat_path)
lines = []
if mat:
    lines.append(f"Loaded {mat.get_name()}, parent={mat.get_editor_property('parent').get_name() if mat.get_editor_property('parent') else 'None'}")
    for s in mat.get_editor_property("scalar_parameter_values"):
        lines.append(f"  Scalar: {s.parameter_info.name} = {s.parameter_value}")
    for v in mat.get_editor_property("vector_parameter_values"):
        lines.append(f"  Vector: {v.parameter_info.name} = {v.parameter_value}")
    for t in mat.get_editor_property("texture_parameter_values"):
        tex = t.parameter_value
        lines.append(f"  Texture: {t.parameter_info.name} = {tex.get_name() if tex else 'None'}")
else:
    lines.append(f"Failed to load {mat_path}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/mi_planet_params.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
