import unreal

hull_paths = [
    "/Game/Presentation/Ships/SM_PC14_FullMesh",
    "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull",
    "/Game/Presentation/Ships/AssaultFull/SM_Hull",
    "/Game/Presentation/Ships/BastionFull/SM_Hull",
    "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull",
    "/Game/Presentation/Ships/BattleshipFull/SM_Hull",
    "/Game/Presentation/Ships/CarrierFull/SM_Hull",
    "/Game/Presentation/Ships/CruiserFull/SM_Hull",
    "/Game/Presentation/Ships/DreadnoughtFull/SM_Hull",
    "/Game/Presentation/Ships/FighterFull/SM_Hull",
    "/Game/Presentation/Ships/FrigateFull/SM_Hull",
    "/Game/Presentation/Ships/MiningFull/SM_Hull",
    "/Game/Presentation/Ships/Missile_ShipFull/SM_Hull",
    "/Game/Presentation/Ships/RadarFull/SM_Hull",
    "/Game/Presentation/Ships/RepairFull/SM_Hull",
    "/Game/Presentation/Ships/TransportFull/SM_Hull",
    "/Game/Presentation/Ships/VectorFull/SM_Hull",
]

updated = 0
for p in hull_paths:
    mesh = unreal.EditorAssetLibrary.load_asset(p)
    if mesh and isinstance(mesh, unreal.StaticMesh):
        if not mesh.get_editor_property("allow_cpu_access"):
            mesh.set_editor_property("allow_cpu_access", True)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
            updated += 1
            unreal.log(f"Enabled allow_cpu_access on {p}")
        else:
            unreal.log(f"Already allow_cpu_access=True on {p}")

unreal.log(f"Total StaticMeshes updated with allow_cpu_access=True: {updated}")
