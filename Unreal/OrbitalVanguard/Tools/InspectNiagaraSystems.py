import unreal

def inspect_niagara(asset_path):
    system = unreal.EditorAssetLibrary.load_asset(asset_path)
    print(f"Loaded Niagara System: {asset_path} -> {system}")
    if system:
        print(f"Class: {system.get_class().get_name()}")

inspect_niagara("/Game/SpaceshipVFXStarter/VFX/ThrusterTrail/NS_ThrusterTrailAdditive_Seamless")
inspect_niagara("/Game/SpaceshipVFXStarter/VFX/WingTrail/NS_WingTrail")
inspect_niagara("/Game/SpaceshipVFXStarter/VFX/Shield/NS_ShieldStandard")
