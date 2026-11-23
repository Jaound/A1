import unreal

out_lines = []
def inspect_hull_bounds(path):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not mesh:
        out_lines.append(f"Cannot load {path}")
        return
    bounds = mesh.get_bounds()
    out_lines.append(f"Mesh: {path}")
    out_lines.append(f"  Origin: {bounds.origin}")
    out_lines.append(f"  BoxExtent: {bounds.box_extent}")
    for s in mesh.get_editor_property('sockets'):
        out_lines.append(f"  Socket: {s.socket_name}, Loc={s.relative_location}")

inspect_hull_bounds("/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull")
inspect_hull_bounds("/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/hull_bounds_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out_lines))
