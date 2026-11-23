import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Ships/DreadnoughtFull/M_Readable.M_Readable")
lines = []
if mat:
    lines.append(f"Material: {mat.get_name()}")
    # check default texture references
    for p in dir(mat):
        if not p.startswith('_'):
            try:
                v = getattr(mat, p)
                if isinstance(v, (str, int, float, bool)):
                    lines.append(f"  {p}: {v}")
            except:
                pass

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/mat_props.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
