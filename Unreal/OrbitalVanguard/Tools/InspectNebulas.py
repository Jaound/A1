import unreal

lines = []
for name in ["01", "02", "03", "04", "05", "11"]:
    path = f"/Game/StarfieldFree/Materials/Material_Instance/MI_Starfield_Nebula_Free_{name}"
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if mat:
        tex_params = unreal.MaterialEditingLibrary.get_texture_parameter_names(mat)
        vector_params = unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)
        
        textures = {}
        for tp in tex_params:
            val = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mat, tp)
            if val:
                textures[str(tp)] = val.get_name()
                
        colors = {}
        for vp in vector_params:
            c = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(mat, vp)
            if c:
                colors[str(vp)] = (round(c.r, 2), round(c.g, 2), round(c.b, 2), round(c.a, 2))
                
        lines.append(f"=== {name} ===")
        lines.append(f"  Textures: {textures}")
        lines.append(f"  Colors: {colors}")

# Inspect Dreadnought and Battleship bounds
dread = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Ships/DreadnoughtFull/SM_Hull")
if dread:
    b = dread.get_bounds()
    lines.append(f"Dreadnought bounds: origin={b.origin}, extent={b.box_extent}")

battle = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Ships/BattleshipFull/SM_Hull")
if battle:
    b = battle.get_bounds()
    lines.append(f"Battleship bounds: origin={b.origin}, extent={b.box_extent}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/nebula_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
