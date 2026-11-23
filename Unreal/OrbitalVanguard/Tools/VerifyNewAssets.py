import unreal

assets = [
    "/Game/BlackHole/Mesh/SM_BlackHole_Sphere.SM_BlackHole_Sphere",
    "/Game/BlackHole/Mesh/SM_InvertedSphereHQ.SM_InvertedSphereHQ",
    "/Game/BlackHole/Materials/MI_Blackhole_Physical.MI_Blackhole_Physical",
    "/Game/BlackHole/Materials/M_Blackhole.M_Blackhole",
    "/Game/MixedVFX/Materials/M_DissolverMaster.M_DissolverMaster",
    "/Game/MixedVFX/Materials/M_SimpleDissolver.M_SimpleDissolver",
    "/Game/MixedVFX/Materials/M_Slash_Master.M_Slash_Master",
    "/Game/MixedVFX/Meshes/SM_Slash_01.SM_Slash_01",
    "/Game/MixedVFX/SceneAssets/Materials/M_SceneMaterialMaster.M_SceneMaterialMaster",
    "/Game/Presentation/CombatEffects/LaserBeam/SM_Source.SM_Source",
    "/Game/Presentation/CombatEffects/CannonShell/SM_Source.SM_Source",
    "/Game/Presentation/CombatEffects/ImpactBurst/SM_Source.SM_Source",
]

for a in assets:
    obj = unreal.EditorAssetLibrary.load_asset(a)
    print(f"Asset: {a} -> {'LOADED (' + obj.get_class().get_name() + ')' if obj else 'FAILED'}")
