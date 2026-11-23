import unreal

paths = [
    "/Game/Presentation/Effects/CannonShell/SM_Source",
    "/Game/Presentation/Effects/LaserBeam/SM_Source",
    "/Game/Presentation/Effects/ImpactBurst/SM_Source",
    "/Game/Presentation/Effects/ImpactBurst/M_ShieldImpact",
    "/Game/Presentation/Effects/ImpactBurst/M_HullImpact",
    "/Game/Presentation/Effects/ImpactBurst/M_Plume",
    "/Game/Presentation/Effects/M_ShieldBubble",
    "/Game/Presentation/Effects/M_EffectGlow",
    "/Game/SpaceshipVFXStarter/Materials/Instances/MI_FresnelGlow_Standard",
    "/Game/SpaceshipVFXStarter/Materials/Instances/MI_FresnelMeshWF_Standard",
    "/Game/SpaceshipVFXStarter/Materials/Instances/MI_SecondaryGlow",
    "/Game/SpaceshipVFXStarter/Materials/Instances/MI_ThrusterTrail_Additive",
    "/Game/MixedVFX/Materials/MaterialInstances/MI_TranslucentMesh",
    "/Game/MixedVFX/Materials/MaterialInstances/MI_Aura_01",
    "/Game/MixedVFX/Materials/MaterialInstances/MI_Flare",
    "/Game/MixedVFX/Materials/MaterialInstances/MI_Lightning",
    "/Game/MixedVFX/Materials/MaterialInstances/MI_Dissolver_01",
    "/Game/MixedVFX/Materials/M_DissolverMaster",
]

lines = []
for p in paths:
    asset = unreal.EditorAssetLibrary.load_asset(p)
    if not asset:
        lines.append(f"MISSING: {p}")
        continue
    cls = asset.get_class().get_name()
    if isinstance(asset, unreal.StaticMesh):
        b = asset.get_bounds()
        lines.append(f"[StaticMesh] {p} | Origin={b.origin} | BoxExtent={b.box_extent}")
    elif isinstance(asset, unreal.MaterialInterface):
        base_mat = asset.get_base_material()
        bm = base_mat.get_editor_property("blend_mode") if base_mat else "N/A"
        sm = base_mat.get_editor_property("shading_model") if base_mat else "N/A"
        scalars = unreal.MaterialEditingLibrary.get_scalar_parameter_names(asset)
        vectors = unreal.MaterialEditingLibrary.get_vector_parameter_names(asset)
        lines.append(f"[{cls}] {p} | Blend={bm} | Shading={sm} | Scalars={list(scalars)} | Vectors={list(vectors)}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/vfx_mat_details.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
