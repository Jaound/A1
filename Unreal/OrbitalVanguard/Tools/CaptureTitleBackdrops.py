"""
UE 5.8 Editor Python script: captures real in-game SM_Hull ship formations into
/Game/UI/Title/T_TitleBackdrop_*.uasset (16:9 2D textures) for Way B lightweight backdrop mode.

Run from Windows PowerShell:
  E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ^
    "<RepoRoot>\Unreal\OrbitalVanguard\OrbitalVanguard.uproject" ^
    /Game/Maps/ControlRange -ExecCmds="py <RepoRoot>\Unreal\OrbitalVanguard\Tools\CaptureTitleBackdrops.py"
"""

import os
import unreal

OUTPUT_PACKAGE_DIR = "/Game/UI/Title"
RENDER_WIDTH = 1920
RENDER_HEIGHT = 1080

FORMATIONS = [
    {
        "asset_name": "T_TitleBackdrop_FlagshipVanguard",
        "camera_loc": unreal.Vector(-3400.0, -1500.0, 760.0),
        "camera_rot": unreal.Rotator(-10.0, 21.0, 0.0),
        "ships": [
            ("/Game/Presentation/Ships/DreadnoughtFull/SM_Hull", unreal.Vector(280.0, 460.0, 40.0), unreal.Rotator(4.0, -158.0, -6.0), 1.0),
            ("/Game/Presentation/Ships/CarrierFull/SM_Hull", unreal.Vector(1220.0, 940.0, 290.0), unreal.Rotator(3.0, -162.0, -4.0), 0.92),
            ("/Game/Presentation/Ships/BastionFull/SM_Hull", unreal.Vector(-680.0, 820.0, -210.0), unreal.Rotator(2.0, -150.0, -8.0), 0.85),
            ("/Game/Presentation/Ships/VectorFull/SM_Hull", unreal.Vector(-140.0, -260.0, 150.0), unreal.Rotator(5.0, -165.0, -5.0), 0.82),
            ("/Game/Presentation/Ships/FighterFull/SM_Hull", unreal.Vector(520.0, 640.0, 460.0), unreal.Rotator(-3.0, -155.0, -14.0), 0.75),
        ],
    },
    {
        "asset_name": "T_TitleBackdrop_CarrierStrike",
        "camera_loc": unreal.Vector(-3550.0, -1420.0, 840.0),
        "camera_rot": unreal.Rotator(-12.0, 20.0, 0.0),
        "ships": [
            ("/Game/Presentation/Ships/BattlecarrierFull/SM_Hull", unreal.Vector(240.0, 480.0, 60.0), unreal.Rotator(4.0, -160.0, -5.0), 1.0),
            ("/Game/Presentation/Ships/BattleshipFull/SM_Hull", unreal.Vector(980.0, 860.0, -140.0), unreal.Rotator(3.0, -156.0, -6.0), 0.94),
            ("/Game/Presentation/Ships/Air_DefenseFull/SM_Hull", unreal.Vector(-260.0, 220.0, 270.0), unreal.Rotator(2.0, -162.0, -4.0), 0.85),
            ("/Game/Presentation/Ships/SM_PC14_FullMesh", unreal.Vector(-840.0, 760.0, -230.0), unreal.Rotator(0.0, -148.0, -10.0), 0.85),
        ],
    },
    {
        "asset_name": "T_TitleBackdrop_LogisticsConvoy",
        "camera_loc": unreal.Vector(-3100.0, -1350.0, 690.0),
        "camera_rot": unreal.Rotator(-9.0, 22.0, 0.0),
        "ships": [
            ("/Game/Presentation/Ships/TransportFull/SM_Hull", unreal.Vector(220.0, 440.0, 20.0), unreal.Rotator(2.0, -158.0, -3.0), 1.0),
            ("/Game/Presentation/Ships/MiningFull/SM_Hull", unreal.Vector(840.0, 820.0, -110.0), unreal.Rotator(3.0, -162.0, -4.0), 0.95),
            ("/Game/Presentation/Ships/CruiserFull/SM_Hull", unreal.Vector(-460.0, 660.0, 190.0), unreal.Rotator(4.0, -154.0, -6.0), 0.88),
            ("/Game/Presentation/Ships/FrigateFull/SM_Hull", unreal.Vector(-220.0, -120.0, -160.0), unreal.Rotator(2.0, -166.0, -5.0), 0.85),
            ("/Game/Presentation/Ships/RepairFull/SM_Hull", unreal.Vector(980.0, 280.0, 220.0), unreal.Rotator(1.0, -160.0, -3.0), 0.85),
        ],
    },
]


def capture_formation(world, formation):
    spawned = []
    stage_origin = unreal.Vector(0.0, 0.0, -12000000.0)
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    try:
        sky_mesh = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Environment/SpaceSkybox/SM_Source")
        if sky_mesh:
            sky_actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, stage_origin, unreal.Rotator())
            sky_actor.static_mesh_component.set_static_mesh(sky_mesh)
            sky_actor.set_actor_scale3d(unreal.Vector(120.0, 120.0, 120.0))
            spawned.append(sky_actor)

        for mesh_path, rel_loc, rel_rot, scale in formation["ships"]:
            mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
            if not mesh:
                unreal.log_warning(f"Missing ship mesh: {mesh_path}")
                continue
            ship_actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, stage_origin + rel_loc, rel_rot)
            ship_actor.static_mesh_component.set_static_mesh(mesh)
            ship_actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
            spawned.append(ship_actor)

        cam_actor = actor_sub.spawn_actor_from_class(
            unreal.SceneCapture2D,
            stage_origin + formation["camera_loc"],
            formation["camera_rot"],
        )
        spawned.append(cam_actor)

        rt = unreal.RenderingLibrary.create_render_target2d(world, RENDER_WIDTH, RENDER_HEIGHT, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        cap = cam_actor.capture_component2d
        cap.texture_target = rt
        cap.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        cap.fov_angle = 42.0
        cap.capture_scene()

        out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "TitleBackdrops")
        os.makedirs(out_dir, exist_ok=True)
        unreal.RenderingLibrary.export_render_target(world, rt, out_dir, f"{formation['asset_name']}.png")

        task = unreal.AssetImportTask()
        task.filename = os.path.join(out_dir, f"{formation['asset_name']}.png")
        task.destination_path = OUTPUT_PACKAGE_DIR
        task.destination_name = formation["asset_name"]
        task.replace_existing = True
        task.automated = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        unreal.log(f"OV TitleBackdrop captured: {OUTPUT_PACKAGE_DIR}/{formation['asset_name']}")
    finally:
        for actor in spawned:
            if actor:
                actor_sub.destroy_actor(actor)


def main():
    editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_sub.get_editor_world()
    if not world:
        raise RuntimeError("No editor world loaded. Open /Game/Maps/ControlRange first.")
    for formation in FORMATIONS:
        capture_formation(world, formation)


if __name__ == "__main__":
    main()
