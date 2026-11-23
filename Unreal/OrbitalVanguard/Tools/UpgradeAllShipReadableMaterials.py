import unreal

lib = unreal.MaterialEditingLibrary
editor_asset = unreal.EditorAssetLibrary

ships = [
    "Air_DefenseFull",
    "AssaultFull",
    "BastionFull",
    "BattlecarrierFull",
    "BattleshipFull",
    "CarrierFull",
    "CruiserFull",
    "DreadnoughtFull",
    "FighterFull",
    "FrigateFull",
    "MiningFull",
    "Missile_ShipFull",
    "RadarFull",
    "RepairFull",
    "TransportFull",
    "VectorFull",
]

materials_to_update = [f"/Game/Presentation/Ships/{s}/M_Readable.M_Readable" for s in ships]
materials_to_update.append("/Game/Presentation/PC14Full/M_PC14_FullReadable.M_PC14_FullReadable")

results = []

for mat_path in materials_to_update:
    mat = editor_asset.load_asset(mat_path)
    if not mat:
        results.append(f"MISSING: {mat_path}")
        continue
    
    expressions = lib.get_material_expressions(mat)
    const_node = None
    mult_node = None
    already_has_param = False
    
    for exp in expressions:
        cls_name = exp.get_class().get_name()
        if cls_name == 'MaterialExpressionScalarParameter':
            if exp.get_editor_property('parameter_name') == 'EmissiveFill':
                already_has_param = True
        elif cls_name == 'MaterialExpressionConstant':
            const_node = exp
        elif cls_name == 'MaterialExpressionMultiply':
            pos = lib.get_material_expression_node_position(exp)
            if pos[0] == -100:
                mult_node = exp
    
    if already_has_param:
        results.append(f"ALREADY_UPDATED: {mat_path}")
        continue
        
    if const_node and mult_node:
        lib.delete_material_expression(mat, const_node)
        param = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -350, -300)
        param.set_editor_property('parameter_name', 'EmissiveFill')
        param.set_editor_property('default_value', 0.0)
        lib.connect_material_expressions(param, '', mult_node, 'B')
        lib.recompile_material(mat)
        saved = editor_asset.save_loaded_asset(mat, only_if_is_dirty=False)
        results.append(f"SUCCESS: {mat_path} (saved={saved})")
    else:
        results.append(f"SKIPPED (no match): {mat_path} const={const_node} mult={mult_node}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/upgrade_mat_results.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(results))

print("Upgrade finished.")
