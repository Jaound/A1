"""독립 PIE 창에서 실제 게임 명령과 결과를 검사한다. 마우스 입력은 별도 검사한다."""
import unreal as u
import json
import time
import traceback
from pathlib import Path

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
out=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))/"Validation"/time.strftime("FleetCommands_%Y%m%d_%H%M%S")
out.mkdir(parents=True)
result={"checks":[],"physical_input":False}
def check(name,value):
    if not value: raise AssertionError(name)
    result["checks"].append(name)
def p(obj,name): return obj.get_editor_property(name)
def frames(n=8):
    for _ in range(n): yield

def run():
    while not editor.get_game_world(): yield
    world=editor.get_game_world()
    pc=u.GameplayStatics.get_player_controller(world,0)
    while not pc or not pc.get_hud():
        yield
        pc=u.GameplayStatics.get_player_controller(world,0)
    hud=pc.get_hud()
    scene=pc.get_play_scenario()
    check("50코스트 출격",pc.start_play_scenario("Fleet50"))
    def photo(name):
        path=out/(name+".png")
        yield from frames(30)
        u.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+path.as_posix()+'" -nosuffix',pc)
        until=time.monotonic()+6
        while not path.exists() and time.monotonic()<until:yield
        check(name+" 렌더",path.exists())
    u.SystemLibrary.execute_console_command(world,"showhud",pc)
    carrier=next(s for s in p(pc,"ships") if "항공모함" in p(s,"ship_name") and p(s,"friendly"))
    for key,value in [("camera_focus",carrier.get_actor_location()),("camera_yaw",48.),("camera_pitch",-23.),("camera_distance",max(2800,carrier.get_actor_bounds(False)[1].length()*3.3))]:
        pc.set_editor_property(key,value)
    pc.update_camera()
    yield from photo("Fleet50")
    for brightness,material,label in [(1.0,0,"초계함_기존"),(0.45,1,"초계함_보정")]:
        u.SystemLibrary.execute_console_command(world,f"ov.Presentation.PC14FieldMaterial {material}",pc)
        u.SystemLibrary.execute_console_command(world,f"ov.Presentation.PC14Exposure {brightness}",pc)
        assert pc.start_play_scenario("Fleet50")
        scout=next(s for s in p(pc,"ships") if "초계함" in p(s,"ship_name") and not p(s,"friendly"))
        sys=p(scout,"systems")
        sys.apply_world_hit(scout.get_actor_location(),u.Vector(0,0,1),float(p(sys,"shield")+1),u.OVDamageCause.EXTERNAL)
        for name,value in [("camera_focus",scout.get_actor_location()),("camera_distance",max(1800,scout.get_actor_bounds(False)[1].length()*3.2)),("camera_yaw",40.),("camera_pitch",-22.)]:
            pc.set_editor_property(name,value)
        pc.update_camera()
        pc.update_combat_presentation()
        yield from photo(label)
    u.SystemLibrary.execute_console_command(world,"showhud",pc)
    heroes=p(pc,"heroes")
    for side in [0,1]:
        records=[h for h in heroes.get_heroes(side) if p(h,"runtime_unit_id")>0]
        check(f"세력 {side} 실인원 15명",len(records)==15 and len({p(h,"runtime_person_id") for h in records})==15)
    for _ in range(2):
        check("재시작",pc.start_play_scenario("Fleet100"))
        check("스타팅 영웅 명단 중복 없음",len([h for h in heroes.get_heroes(0) if p(h,"runtime_unit_id")>0])==15)
    ships=list(p(pc,"ships"))
    ally=[s for s in ships if p(s,"friendly")]
    repair=next(x for x in ally if "보급수리함" in p(x,"ship_name"))
    target=next(x for x in ally if "전함" in p(x,"ship_name"))
    combat=p(pc,"combat")
    kinds={str(p(p(m,"profile"),"kind")) for m in p(combat.get_ship_state(target),"mounts")}
    check("비전용 일반·유도 모듈 장착",any("GUIDED_MISSILE" in k for k in kinds) and any("GUIDED" not in k and "MISSILE" in k for k in kinds))
    log=next(x for x in u.ObjectIterator(u.OVLogisticsSubsystem) if x.get_outer()==world)
    hud.execute_ui_action("BattleFeedToggle")
    yield from frames()
    hud.execute_ui_action("NoticeSettings")
    yield from frames()
    check("17종 사건별 표시 설정",all(hud.get_action_center("ToastKind"+str(i)).x>=0 for i in range(17)))
    hud.execute_ui_action("NoticeSettings")
    hud.execute_ui_action("BattleFeedToggle")
    hud.execute_ui_action("SceneMenu")
    yield from frames()
    check("8개 임무 마지막 행 표시",hud.get_action_center("Sandbox").x>=0)
    hud.execute_ui_action("SceneMenuClose")
    pc.select_ship(repair)
    check("수리함 초도 보급","2746.0 / 2746.0" in hud.get_selected_cargo_label())
    for s in ships: s.set_automation(False,False);s.stop_movement()
    enemy=next(s for s in ships if not p(s,"friendly"))
    hud.execute_ui_action("RepairOrder"); pc.action("Pick_"+str(ships.index(enemy)))
    check("적 수리 거절",not p(log,"repair_links") and "수리 불가" in p(pc,"last_action"))
    systems=p(target,"systems")
    systems.apply_world_hit(target.get_actor_location(),u.Vector(0,0,1),float(p(systems,"max_shield")+p(systems,"max_hull_hp")*.12),u.OVDamageCause.EXTERNAL)
    hp0=p(systems,"hull_hp")
    check("실제 손상 표적",0<hp0<p(systems,"max_hull_hp"))
    repair.set_actor_location(target.get_actor_location()+u.Vector(70000,15000,0),False,False)
    start=repair.get_actor_location()
    hud.execute_ui_action("RepairOrder");pc.action("Pick_"+str(ships.index(target)))
    pc.action("Pause")
    for _ in range(1700):
        pc.step_simulation(.20)
        if p(systems,"hull_hp")>hp0+2:break
    check("원거리 접근 후 유상 수리", (repair.get_actor_location()-start).length()>1000 and p(systems,"hull_hp")>hp0 and "2746.0 / 2746.0" not in hud.get_selected_cargo_label())
    pc.action("Stop")
    check("정지 명령이 수리 취소",not p(log,"repair_links"))
    pc.action("Pause")
    yield from frames()
    yield from photo("수리함_화물")
    pc.select_ship(target)
    hud.execute_ui_action("PlotJump")
    check("목적지 전에는 충전 없음",not p(target,"ftl_charging"))
    hud.execute_ui_action("StarSystem_Select_vega_mining")
    hud.execute_ui_action("StarSystem_FTL_vega_mining")
    check("목적지 선택 후 충전",p(target,"ftl_charging"))
    check("지도 명령 후 선택 유지",p(pc,"selected")==target)
    for _ in range(12):pc.step_simulation(.25)
    check("정지 중 도약 보류",not combat.is_departed(target))
    pc.action("Pause")
    for _ in range(68):pc.step_simulation(.25)
    check("충전 후 선택함 이탈",combat.is_departed(target) and not combat.is_departed(repair))

def tick(dt):
    try:
        next(job)
        return
    except StopIteration:
        result["passed"]=True
    except Exception:
        result.update(passed=False,error=traceback.format_exc())
    (out/"result.json").write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf8")
    u.unregister_slate_post_tick_callback(handle)
    u.OVDevelopmentEditorLibrary.restore_preview_settings()
    u.SystemLibrary.quit_editor()

job=run()
assert u.OVDevelopmentEditorLibrary.start_preview_window(1280,740)
handle=u.register_slate_post_tick_callback(tick)
