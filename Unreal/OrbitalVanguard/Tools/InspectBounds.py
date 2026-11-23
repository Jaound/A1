import unreal

lines = []
for path in [
    "/Game/Planet_Project/Assets/Static_Meshes/SM_Earth_Planet",
    "/Game/Planet_Project/Assets/Static_Meshes/SM_Custom_Planet",
    "/Engine/BasicShapes/Sphere",
    "/Game/StarfieldFree/Mesh/SM_StarSphereFree",
    "/Game/StarfieldFree/Mesh/SM_Nebula_Sphere_Free"
]:
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if mesh:
        bounds = mesh.get_bounds()
        lines.append(f"{path}: box_extent={bounds.box_extent}, sphere_radius={bounds.sphere_radius}")
    else:
        lines.append(f"{path}: NOT FOUND")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/bounds_out.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
