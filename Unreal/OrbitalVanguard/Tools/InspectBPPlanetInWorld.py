import unreal

world = unreal.EditorLevelLibrary.get_editor_world()
bp_cls = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Planet_Project/Blueprints/BP_Planet")
lines = []
if bp_cls and world:
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(bp_cls, unreal.Vector(0,0,0), unreal.Rotator(0,0,0))
    if actor:
        lines.append(f"Spawned BP_Planet: {actor.get_name()}")
        comps = actor.get_components_by_class(unreal.ActorComponent)
        for c in comps:
            lines.append(f"  Component: {c.get_name()} ({c.get_class().get_name()})")
            if isinstance(c, unreal.StaticMeshComponent):
                sm = c.get_editor_property("static_mesh")
                lines.append(f"    Mesh: {sm.get_path_name() if sm else 'None'}")
                lines.append(f"    Scale: {c.get_editor_property('relative_scale3d')}")
                for mi in range(c.get_num_materials()):
                    mat = c.get_material(mi)
                    lines.append(f"    Mat {mi}: {mat.get_path_name() if mat else 'None'}")
        actor.destroy_actor()

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/bp_planet_spawned.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
