import unreal

editor_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map("/Game/Planet_Project/Maps/Lv_Planet_Project")

world = unreal.EditorLevelLibrary.get_editor_world()
lines = []
lines.append(f"World: {world.get_name()}")
actors = unreal.EditorLevelLibrary.get_all_level_actors()
for a in actors:
    lines.append(f"Actor: {a.get_name()} ({a.get_class().get_name()}) at {a.get_actor_location()}, Rot={a.get_actor_rotation()}")
    if isinstance(a, unreal.DirectionalLight):
        comp = a.get_component_by_class(unreal.DirectionalLightComponent)
        if comp:
            lines.append(f"  Light: Intensity={comp.intensity}, Color={comp.light_color}, Rot={a.get_actor_rotation()}")
    elif isinstance(a, unreal.SkyLight):
        comp = a.get_component_by_class(unreal.SkyLightComponent)
        if comp:
            lines.append(f"  SkyLight: Intensity={comp.intensity}, Color={comp.light_color}")
    elif isinstance(a, unreal.StaticMeshActor):
        comp = a.static_mesh_component
        if comp and comp.static_mesh:
            lines.append(f"  StaticMesh: {comp.static_mesh.get_name()}, Mat={comp.get_material(0).get_name() if comp.get_material(0) else 'None'}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/level_planet_inspect.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
