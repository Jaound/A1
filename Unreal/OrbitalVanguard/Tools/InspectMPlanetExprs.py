import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/M_Planet")
lines = []
if mat:
    exprs = unreal.MaterialEditingLibrary.get_material_expressions(mat)
    lines.append(f"Num expressions: {len(exprs)}")
    for e in exprs:
        desc = f"{e.get_class().get_name()}: {e.get_name()}"
        if isinstance(e, unreal.MaterialExpressionTextureSampleParameter2D):
            desc += f" (Param: {e.get_editor_property('parameter_name')}, Tex: {e.texture.get_name() if e.texture else 'None'})"
        elif isinstance(e, unreal.MaterialExpressionScalarParameter):
            desc += f" (Param: {e.get_editor_property('parameter_name')}, Val: {e.get_editor_property('default_value')})"
        elif isinstance(e, unreal.MaterialExpressionVectorParameter):
            desc += f" (Param: {e.get_editor_property('parameter_name')}, Val: {e.get_editor_property('default_value')})"
        lines.append(desc)

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/m_planet_exprs.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
