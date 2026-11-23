import unreal

tex = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Ships/DreadnoughtFull/T_Base.T_Base")
lines = []
if tex:
    lines.append(f"T_Base: {tex.get_name()}, size=({tex.blueprint_get_size_x()}, {tex.blueprint_get_size_y()})")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/tex_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
