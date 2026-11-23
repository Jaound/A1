import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/M_Planet")
lines = []
if mat:
    lines.append(f"Material: {mat.get_name()}")
    # Inspect all expressions in M_Planet
    for expr in mat.get_editor_property("expressions"):
        lines.append(f"  Expr: {expr.get_class().get_name()} -> {expr.get_name()}")
        if expr.get_class().get_name() == "MaterialExpressionVectorParameter":
            lines.append(f"    ParamName: {expr.get_editor_property('parameter_name')}")
        elif expr.get_class().get_name() == "MaterialExpressionScalarParameter":
            lines.append(f"    ParamName: {expr.get_editor_property('parameter_name')}, Default: {expr.get_editor_property('default_value')}")
else:
    lines.append("M_Planet not found")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/m_planet_nodes.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
