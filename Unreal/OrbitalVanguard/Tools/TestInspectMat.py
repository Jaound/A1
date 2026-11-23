import unreal

lib = unreal.MaterialEditingLibrary
mat_path = "/Game/Presentation/Ships/DreadnoughtFull/M_Readable.M_Readable"
mat = unreal.EditorAssetLibrary.load_asset(mat_path)
lines = [f"Loaded {mat_path}: {mat}"]

expressions = lib.get_material_expressions(mat)
lines.append(f"Expression count: {len(expressions)}")
for exp in expressions:
    line = f"  Expr: {exp.get_class().get_name()} pos={lib.get_material_expression_node_position(exp)}"
    if exp.get_class().get_name() == 'MaterialExpressionScalarParameter':
        line += f" param={exp.get_editor_property('parameter_name')}"
    elif exp.get_class().get_name() == 'MaterialExpressionVectorParameter':
        line += f" param={exp.get_editor_property('parameter_name')}"
    elif exp.get_class().get_name() == 'MaterialExpressionConstant':
        line += f" val={exp.get_editor_property('r')}"
    lines.append(line)

input_node = lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lines.append(f"MP_EMISSIVE_COLOR input node: {input_node.get_class().get_name() if input_node else 'None'}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/inspect_mat_out.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
