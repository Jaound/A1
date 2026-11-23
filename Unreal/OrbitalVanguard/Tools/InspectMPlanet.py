import unreal

m = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/M_Planet")
lines = []
if m:
    lines.append(f"Name: {m.get_name()}")
    lines.append(f"Blend Mode: {m.get_editor_property('blend_mode')}")
    lines.append(f"Shading Model: {m.get_editor_property('shading_model')}")
    lines.append(f"Two Sided: {m.get_editor_property('two_sided')}")
else:
    lines.append("M_Planet not found")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/m_planet_props.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
