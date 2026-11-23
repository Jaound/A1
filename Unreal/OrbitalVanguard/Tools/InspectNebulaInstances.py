import unreal

out = []
for i in [1, 2, 3, 4, 5, 11]:
    p = f"/Game/StarfieldFree/Materials/Material_Instance/MI_Starfield_Nebula_Free_{i:02d}.MI_Starfield_Nebula_Free_{i:02d}"
    mat = unreal.EditorAssetLibrary.load_asset(p)
    if mat:
        out.append(f"Nebula MI {i}:")
        for s in mat.get_editor_property("scalar_parameter_values"):
            out.append(f"  {s.parameter_info.name} = {s.parameter_value}")
        for t in mat.get_editor_property("texture_parameter_values"):
            tex = t.parameter_value
            out.append(f"  {t.parameter_info.name} = {tex.get_name() if tex else 'None'}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/nebula_instances_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
