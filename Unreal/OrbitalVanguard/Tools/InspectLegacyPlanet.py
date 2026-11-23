import unreal

m_leg = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Environment/SpaceSkybox/M_Planet")
lines = []
if m_leg:
    lines.append(f"Legacy M_Planet: {m_leg.get_name()}, Class: {m_leg.get_class().get_name()}")
    lines.append(f"  Blend Mode: {m_leg.get_editor_property('blend_mode')}")
    lines.append(f"  Shading Model: {m_leg.get_editor_property('shading_model')}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/legacy_planet_inspect.txt", "w") as f:
    f.write("\n".join(lines))
