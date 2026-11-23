import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Ships/DreadnoughtFull/M_Readable.M_Readable")
lines = []
if mat:
    lines.append(f"M_Readable: class={mat.get_class().get_name()}")
    # List expressions
    for exp in mat.get_editor_property("expressions"):
        lines.append(f"  Expr: {exp.get_class().get_name()}")
        if exp.get_class().get_name() == 'MaterialExpressionTextureSample':
            tex = exp.get_editor_property('texture')
            lines.append(f"    Texture: {tex.get_name() if tex else 'None'}")
        elif exp.get_class().get_name() == 'MaterialExpressionVectorParameter':
            pname = exp.get_editor_property('parameter_name')
            lines.append(f"    VectorParam: {pname}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/m_readable_graph.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
