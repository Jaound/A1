import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Blueprints/BP_Planet")
gen = bp.generated_class()
cdo = unreal.get_default_object(gen)
lines = []
for comp in cdo.get_editor_property("root_component").get_children_components(True):
    lines.append(f"Comp: {comp.get_name()} ({comp.get_class().get_name()})")
    if isinstance(comp, unreal.StaticMeshComponent):
        lines.append(f"  Mesh: {comp.static_mesh.get_path_name() if comp.static_mesh else 'None'}")
        lines.append(f"  Mat0: {comp.get_material(0).get_path_name() if comp.get_material(0) else 'None'}")

# Also check root component itself:
root = cdo.get_editor_property("root_component")
lines.append(f"Root: {root.get_name()} ({root.get_class().get_name()})")
if isinstance(root, unreal.StaticMeshComponent):
    lines.append(f"  Mesh: {root.static_mesh.get_path_name() if root.static_mesh else 'None'}")
    lines.append(f"  Mat0: {root.get_material(0).get_path_name() if root.get_material(0) else 'None'}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/bp_planet_comps.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
