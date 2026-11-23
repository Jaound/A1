import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Blueprints/BP_Planet")
lines = []
if bp:
    lines.append(f"BP_Planet loaded: {bp.get_name()}")
    gen_cls = bp.generated_class()
    cdo = unreal.get_default_object(gen_cls)
    lines.append(f"CDO: {cdo.get_name()}")
    
    # List components
    comps = cdo.get_components_by_class(unreal.ActorComponent)
    for c in comps:
        lines.append(f"Component: {c.get_name()} ({c.get_class().get_name()})")
        if isinstance(c, unreal.StaticMeshComponent):
            sm = c.get_editor_property("static_mesh")
            lines.append(f"  Mesh: {sm.get_path_name() if sm else 'None'}")
            lines.append(f"  RelativeScale: {c.get_editor_property('relative_scale3d')}")
            lines.append(f"  RelativeLoc: {c.get_editor_property('relative_location')}")
            for mi in range(c.get_num_materials()):
                mat = c.get_material(mi)
                lines.append(f"  Mat {mi}: {mat.get_path_name() if mat else 'None'}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/bp_planet_structure.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
