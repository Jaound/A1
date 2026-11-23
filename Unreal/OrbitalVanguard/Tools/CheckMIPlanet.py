import unreal

mi = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/MI_Planet")
mp = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/M_Planet")

lines = []
lines.append(f"MI_Planet parent: {mi.get_editor_property('parent').get_name() if mi else 'None'}")
# Check texture parameter values in MI_Planet
for tex_name in unreal.MaterialEditingLibrary.get_texture_parameter_names(mi):
    tex_val = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mi, tex_name)
    lines.append(f"Texture {tex_name} = {tex_val.get_path_name() if tex_val else 'None'}")

# Check scalar parameter values in MI_Planet
for s_name in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mi):
    s_val = unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi, s_name)
    lines.append(f"Scalar {s_name} = {s_val}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/check_mi_planet.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
