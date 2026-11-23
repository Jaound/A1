"""대표 6종의 실제 전장 렌더와 쉴드 크기를 기록한다. 조작은 PIE API로 수행한다."""
import json
import struct
import time
import traceback
from pathlib import Path
import unreal as u

level = u.get_editor_subsystem(u.LevelEditorSubsystem)
editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation" / ("HullPresentation_" + time.strftime("%Y%m%d_%H%M%S"))
out.mkdir(parents=True)
report = {"ships": {}, "direct_input": False}
started = time.monotonic()

def frames(n):
    for _ in range(n):
        yield

def capture_preview(world, pc, path):
    # Slate can briefly target the outer editor instead of the floating PIE window.
    for attempt in range(3):
        output=path.with_stem(path.stem+f"_attempt{attempt+1}")
        u.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+output.as_posix()+'" -nosuffix',pc)
        deadline=time.monotonic()+6
        while not output.exists() and time.monotonic()<deadline: yield
        if output.exists():
            with output.open("rb") as stream: header=stream.read(24)
            if len(header)==24:
                width,height=struct.unpack(">II",header[16:24])
                if 1280<=width<=1320 and 720<=height<=800:
                    output.rename(path)
                    return
        settle=time.monotonic()+1.5
        while time.monotonic()<settle: yield
    raise RuntimeError("별도 플레이 창 캡처 실패: "+path.name)


def verify_materials():
    material_paths=[p for p in u.EditorAssetLibrary.list_assets("/Game/Presentation/WeaponsFull") if p.endswith("/M_Source.M_Source")]
    assert len(material_paths)==9
    for path in material_paths:
        mat=u.load_asset(path)
        lib=u.MaterialEditingLibrary
        for prop in ("BASE_COLOR","NORMAL","METALLIC","ROUGHNESS"):
            assert lib.get_material_property_input_node(mat,getattr(u.MaterialProperty,"MP_"+prop)),path
        emission=lib.get_material_property_input_node(mat,u.MaterialProperty.MP_EMISSIVE_COLOR)
        fills=[n.get_editor_property("r") for n in lib.get_inputs_for_material_expression(mat,emission) if isinstance(n,u.MaterialExpressionConstant)]
        assert len(fills)==1 and abs(fills[0]-.025)<.0001,(path,fills)
    report["weapon_materials_checked"]=len(material_paths)

def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = u.GameplayStatics.get_player_controller(world,0)
    while not pc or not pc.get_hud():
        yield
        pc = u.GameplayStatics.get_player_controller(world,0)
    assert pc.start_play_scenario("Fleet100")
    pillars=[a.get_name() for a in u.GameplayStatics.get_all_actors_of_class(world,u.StaticMeshActor)
             if abs(a.get_actor_location().x)==6000 and abs(a.get_actor_location().y)==6000 and a.get_actor_scale3d().z==12]
    assert not pillars,pillars
    report["remaining_beacons"] = pillars
    dev = pc.get_editor_property("development")
    pc.get_hud().set_details_open(False)
    u.SystemLibrary.execute_console_command(world,"showhud",pc)
    for key in ("frigate","vector","battleship","dreadnought","repair","carrier"):
        pc.set_editor_property("developer_mode",True)
        ship, = dev.spawn_ships(key,0,1,u.Vector(-140000,100000,2000),0,0,0,False,False)
        pc.set_editor_property("developer_mode",False)
        pc.select_ship(ship)
        systems=ship.get_editor_property("systems")
        layout=systems.get_editor_property("layout")
        axes=layout.get_editor_property("fitted_shield_radii")
        extent=layout.get_editor_property("local_half_extent")
        pc.set_editor_property("camera_focus",ship.get_actor_location())
        pc.set_editor_property("camera_distance",max(1100,axes.x*ship.get_actor_scale3d().x*1.65))
        pc.set_editor_property("camera_yaw",-80)
        pc.set_editor_property("camera_pitch",-13)
        pc.update_camera()
        yield from frames(16)
        report["ships"][key]={"shield_axes_cm":[axes.x,axes.y,axes.z],"hull_half_extent_cm":[extent.x,extent.y,extent.z],
                              "mount_count":len(ship.get_weapon_visual_states()),"shield_active":systems.get_editor_property("shield_active")}
        path=out/(key+".png")
        yield from capture_preview(world,pc,path)
        if key in ("frigate","battleship","dreadnought"):
            # Isolate lighting on this disposable preview ship; no persisted game setting changes.
            tuning=systems.get_editor_property("tuning")
            tuning.set_editor_property("allow_manual_shield_toggle",True)
            systems.set_editor_property("tuning",tuning)
            assert systems.set_shield_manual_off(True)
            yield from frames(5)
            assert not systems.get_editor_property("shield_active")
            bare=out/(key+"_unshielded.png")
            yield from capture_preview(world,pc,bare)
        pc.set_editor_property("developer_mode",True)
        assert dev.delete_ship(ship)
    pc.set_editor_property("developer_mode",False)
    # Draw only real launched attacks and read their world-space mesh instances.
    assert pc.start_calibration_range(2)
    combat=pc.get_editor_property("combat")
    ships=pc.get_editor_property("ships")
    target=ships[2]
    pc.set_editor_property("developer_mode",True)
    cruiser,=dev.spawn_ships("cruiser",0,1,u.Vector(0,8000,2500),0,0,0,False,True)
    pc.set_editor_property("developer_mode",False)
    for ship in (ships[0],ships[1],cruiser):
        ship.set_automation(False,True)
        combat.set_target(ship,target)
    target.set_automation(False,True)
    combat.set_target(target,cruiser)
    if pc.get_editor_property("simulation_paused"): pc.action("Pause")
    lengths={}
    for step in range(900):
        pc.step_simulation(.05)
        pc.update_combat_presentation()
        for prop,axis in [("cannon_projectile_meshes","z"),("standard_missile_meshes","x"),("anti_ship_missile_meshes","x")]:
            comp=pc.get_editor_property(prop)
            if comp.get_instance_count()==0: continue
            transform=comp.get_instance_transform(0,True)
            box=comp.static_mesh.get_bounding_box()
            lengths[prop]=getattr(box.max-box.min,axis)*getattr(transform.scale3d,axis)
        if len(lengths)==3:break
        if step%30==0:yield
    report["projectile_lengths_cm"]=lengths
    report["combat_shots"]=combat.get_editor_property("shot_count")
    assert len(lengths)==3 and lengths["cannon_projectile_meshes"]<min(lengths["standard_missile_meshes"],lengths["anti_ship_missile_meshes"]),lengths


def tick(dt):
    try:
        if time.monotonic()-started>150: raise TimeoutError()
        next(steps)
        return
    except StopIteration:
        report["passed"]=True
    except Exception:
        report["passed"]=False
        report["error"]=traceback.format_exc()
    (out/"result.json").write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf8")
    u.unregister_slate_post_tick_callback(handle)
    u.OVDevelopmentEditorLibrary.restore_preview_settings()
    u.SystemLibrary.quit_editor()

verify_materials()
steps=run()
assert u.OVDevelopmentEditorLibrary.start_preview_window(1280, 720)
handle=u.register_slate_post_tick_callback(tick)
