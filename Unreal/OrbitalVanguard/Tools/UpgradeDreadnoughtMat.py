import unreal

lib = unreal.MaterialEditingLibrary
mat_path = "/Game/Presentation/Ships/DreadnoughtFull/M_Readable.M_Readable"
mat = unreal.EditorAssetLibrary.load_asset(mat_path)

expressions = lib.get_material_expressions(mat)
const_node = None
mult_node = None
for exp in expressions:
    if exp.get_class().get_name() == 'MaterialExpressionConstant':
        const_node = exp
    elif exp.get_class().get_name() == 'MaterialExpressionMultiply' and lib.get_material_expression_node_position(exp)[0] == -100:
        mult_node = exp

print(f"const_node: {const_node}, mult_node: {mult_node}")

if const_node and mult_node:
    # Delete the constant node
    lib.delete_material_expression(mat, const_node)
    
    # Create ScalarParameter EmissiveFill
    param = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -350, -300)
    param.set_editor_property('parameter_name', 'EmissiveFill')
    param.set_editor_property('default_value', 0.0)
    
    # Connect param to mult_node input B
    lib.connect_material_expressions(param, '', mult_node, 'B')
    
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    print("Successfully replaced Constant with EmissiveFill parameter and recompiled!")
