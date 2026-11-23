"""전장의 일반/블랙홀/복귀 장면을 별도 PIE 창에서 촬영한다."""
import json
import math
import struct
import time
import traceback
from pathlib import Path
import unreal as u

out=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))/"Validation"/time.strftime("BattleSky_%Y%m%d_%H%M%S")
out.mkdir(parents=True)
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
report={"captures":[],"physical_input":False}
started=time.monotonic()

def capture(world,pc,label):
    for attempt in range(3):
        until=time.monotonic()+1.5
        while time.monotonic()<until: yield
        path=out/(label+str(attempt)+".png")
        u.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+path.as_posix()+'" -nosuffix',pc)
        until=time.monotonic()+6
        while not path.exists() and time.monotonic()<until: yield
        if path.exists():
            with path.open("rb") as stream: width,height=struct.unpack(">II",stream.read(24)[16:24])
            if 1280<=width<1320 and 720<=height<800:
                report["captures"].append(path.name)
                return
    raise RuntimeError("플레이 창 캡처 실패 "+label)

def run():
    while not editor.get_game_world(): yield
    world=editor.get_game_world()
    pc=u.GameplayStatics.get_player_controller(world,0)
    while not pc or not pc.get_hud():
        yield
        pc=u.GameplayStatics.get_player_controller(world,0)
    def sky():
        return pc.get_editor_property("space_skybox_mesh")
    def lens():
        return pc.get_editor_property("black_hole_meshes")
    def pose(yaw,pitch,distance,focus):
        for key,value in (("camera_focus",focus),("camera_yaw",yaw),("camera_pitch",pitch),("camera_distance",distance)):
            pc.set_editor_property(key,value)
        pc.update_camera()
        pc.update_combat_presentation()
    assert pc.start_play_scenario("Fleet100")
    u.SystemLibrary.execute_console_command(world,"showhud",pc)
    assert sky().get_material(0).get_name()=="M_MilkyWaySky"
    assert not lens().is_visible()
    focus=u.Vector(0,0,2000)
    yield from capture(world,pc,"일반_전장")
    pose(178,-14,152000,focus)
    yield from capture(world,pc,"일반_회전")
    # Aim through the centre of the distant planet, then use exactly the same sightline for E1.
    camera = pc.get_editor_property("range_camera")
    planet = pc.get_editor_property("distant_planet_mesh")
    eye = camera.get_actor_location()
    direction = planet.get_world_location() - (focus*.18 + eye*.82)
    yaw = math.degrees(math.atan2(direction.y,direction.x))
    pitch = math.degrees(math.atan2(direction.z,math.hypot(direction.x,direction.y)))
    pose(yaw,pitch,95000,focus)
    yield from capture(world,pc,"같은방향_일반행성")
    assert pc.start_play_scenario("BlackHoleTrial")
    pc.update_combat_presentation()
    assert lens().is_visible() and lens().get_material(0).get_name()=="MI_Blackhole_Physical"
    assert not isinstance(lens(),u.InstancedStaticMeshComponent)
    hazards=u.GameplayStatics.get_all_actors_of_class(world,u.OVBlackHoleHazardActor)
    assert len(hazards)==1
    assert not any(c.static_mesh and c.is_visible() for c in hazards[0].get_components_by_class(u.StaticMeshComponent))
    assert (lens().get_world_location()-hazards[0].get_actor_location()).length()<.1
    yield from capture(world,pc,"E1_시작")
    # Oblique/low angle reveals both the accretion disk and lensed far side.
    pose(-65,-52,315000,focus)
    yield from capture(world,pc,"E1_강착원반")
    pose(yaw,pitch,95000,focus)
    eye = camera.get_actor_location()
    def depth(component):
        center, _, radius = u.SystemLibrary.get_component_bounds(component)
        return (center-eye).length(), radius
    pd,pr = depth(planet)
    bd,br = depth(lens())
    assert pd-pr > bd+br
    report["background_depth_gap_cm"] = pd-pr-bd-br
    yield from capture(world,pc,"같은방향_E1중심")
    report["orbits"] = []
    for playgame3 in (True,False):
        if pc.get_editor_property("play_game3") != playgame3: pc.action("CameraProfile")
        pose(0,0,95000,focus)
        for angle in (45,89,91,179,181,269,271,360):
            last = report["orbits"][-1]["angle"] if angle!=45 else 0
            sensitivity = .28 if playgame3 else .3
            pc.apply_camera_input(u.Vector2D(0,-(angle-last)/sensitivity),u.Vector(),0,0,1/60)
            offset = camera.get_actor_location()-focus
            assert abs(offset.length()-95000)<1 and abs(offset.z/95000+math.sin(math.radians(angle)))<.0001
            up = camera.get_actor_up_vector()
            assert abs(up.z-math.cos(math.radians(angle)))<.0001
            report["orbits"].append({"playgame3":playgame3,"angle":angle,"height_cm":round(offset.z,1)})
            if playgame3 and angle==45:
                yield from capture(world,pc,"전장_아래쪽")
    if not pc.get_editor_property("play_game3"): pc.action("CameraProfile")
    pose(65,-26,1500000,focus)
    yield from capture(world,pc,"E1_원경")
    assert pc.start_play_scenario("Fleet100")
    pc.update_combat_presentation()
    assert not lens().is_visible() and not u.GameplayStatics.get_all_actors_of_class(world,u.OVBlackHoleHazardActor)
    report["returned_to_normal"]=True
    pc.start_combat_range()
    pc.action("Env_BlackHole")
    pc.update_combat_presentation()
    assert lens().is_visible()
    pc.action("Env_None")
    pc.update_combat_presentation()
    assert not lens().is_visible()
    report["manual_environment_toggle"]=True

def tick(dt):
    try:
        if time.monotonic()-started>180: raise TimeoutError()
        next(steps)
        return
    except StopIteration: report["passed"]=True
    except Exception:
        report["passed"]=False
        report["error"]=traceback.format_exc()
    (out/"result.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf8")
    u.unregister_slate_post_tick_callback(handle)
    u.OVDevelopmentEditorLibrary.restore_preview_settings()
    u.SystemLibrary.quit_editor()

steps=run()
assert u.OVDevelopmentEditorLibrary.start_preview_window(1280,720)
handle=u.register_slate_post_tick_callback(tick)
