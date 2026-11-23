import unreal

out_lines = []
def inspect_bp(path):
    bp = unreal.EditorAssetLibrary.load_asset(path)
    if not bp:
        out_lines.append(f"Cannot load {path}")
        return
    gen_cls = bp.generated_class()
    cdo = unreal.get_default_object(gen_cls)
    out_lines.append(f"BP: {path}, class={cdo.get_class().get_name()}")
    for sc in cdo.get_components_by_class(unreal.StaticMeshComponent):
        sm = sc.get_editor_property("static_mesh")
        mat = sc.get_material(0)
        out_lines.append(f"  Component: {sc.get_name()}, Mesh={sm.get_name() if sm else 'None'}, Mat={mat.get_name() if mat else 'None'}")

inspect_bp("/Game/StarfieldFree/BluePrint/BP_Starfield_bgNebula_Free.BP_Starfield_bgNebula_Free")
inspect_bp("/Game/StarfieldFree/BluePrint/BP_Starfield_Nebula_Free.BP_Starfield_Nebula_Free")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/starfield_bp_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out_lines))
