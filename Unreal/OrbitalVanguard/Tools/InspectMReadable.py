import unreal

out_lines = []
def inspect_mat(path):
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if not mat:
        out_lines.append(f"Cannot load {path}")
        return
    out_lines.append(f"Material: {path}, class={mat.get_class().get_name()}")
    try:
        parent = mat.get_editor_property('parent') if mat.get_class().get_name() == 'MaterialInstanceConstant' else None
        out_lines.append(f"  Parent: {parent.get_name() if parent else 'None'}")
        if parent:
            for s in mat.get_editor_property("scalar_parameter_values"):
                out_lines.append(f"  Scalar: {s.parameter_info.name} = {s.parameter_value}")
            for v in mat.get_editor_property("vector_parameter_values"):
                out_lines.append(f"  Vector: {v.parameter_info.name} = {v.parameter_value}")
            for t in mat.get_editor_property("texture_parameter_values"):
                tex = t.parameter_value
                out_lines.append(f"  Texture: {t.parameter_info.name} = {tex.get_name() if tex else 'None'}")
    except Exception as e:
        out_lines.append(f"  Error: {e}")

inspect_mat("/Game/Presentation/Ships/DreadnoughtFull/M_Readable.M_Readable")
inspect_mat("/Game/Presentation/Ships/CarrierFull/M_Readable.M_Readable")
inspect_mat("/Game/StarfieldFree/Materials/Material_Instance/MI_Starfield_Nebula_Free_01.MI_Starfield_Nebula_Free_01")
inspect_mat("/Game/SpaceshipVFXStarter/Demo/DemoMaterials/MM_SciFiShip.MM_SciFiShip")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/m_readable_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out_lines))
