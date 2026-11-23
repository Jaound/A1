"""별도 PIE에서 공습 파동, 도약, 다중 선택과 손실 판정을 검사한다."""
import json
from pathlib import Path
import time
import traceback
import unreal as u

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation" / time.strftime("AirRaid_%Y%m%d_%H%M%S")
out.mkdir(parents=True)
report = {"checks": [], "physical_input": False}
def p(obj, name):
    return obj.get_editor_property(name)
def check(name, ok):
    if not ok:
        raise AssertionError(name)
    report["checks"].append(name)

def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = u.GameplayStatics.get_player_controller(world, 0)
    while not pc or not pc.get_hud():
        yield
        pc = u.GameplayStatics.get_player_controller(world, 0)
    play = pc.get_play_scenario()
    combat = p(pc, "combat")

    carrier = p(pc,"carrier")
    state = u.OVAircraftState
    outcome = u.OVPlaySceneResult
    def advance(seconds,speed=1):
        for _ in range(int(seconds*4)):
            pc.step_simulation(.25/speed)
    def start():
        assert pc.start_play_scenario("Evacuation")
        return list(p(pc,"ships"))
    def all_friendly():
        return pc.select_fleet_rectangle(u.Vector2D(-10000,-10000),u.Vector2D(10000,10000),False)
    def capture(name):
        until = time.monotonic()+.4
        while time.monotonic()<until: yield
        path = out/(name+".png")
        u.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+path.as_posix()+'" -nosuffix',pc)
        until = time.monotonic()+5
        while not path.exists() and time.monotonic()<until: yield
        check(name+" 렌더",path.exists())
    fleet = start()
    pc.action("Speed1")
    ship = next(x for x in fleet if "항공모함" in p(x,"ship_name"))
    wing = lambda: [a for a in p(carrier,"aircraft") if p(a,"carrier")==ship]
    ids = {p(a,"aircraft_id") for a in wing()}
    check("공습 전 아군 8척과 항모 재고 40기",len(fleet)==8 and len(ids)==40 and p(play,"arrived_waves")==0)
    check("방향 예고", "동쪽" in play.get_status())
    center = next(x for x in fleet if p(combat.get_ship_state(x),"weapon_tier_cap")==4).get_actor_location()
    check("중심 구축전함과 근접 호위",center.length()<2100 and all((x.get_actor_location()-center).length()<40000 for x in fleet))
    # Wait for the local camera to project the new fleet, then exercise the actual rectangle query.
    yield
    check("화면 사각형으로 8척 선택",all_friendly()==8 and all(p(x,"selected") for x in fleet))
    pc.action("Move")
    pc.action("Up")
    pc.action("Confirm")
    check("모든 선택함 동일 이동 벡터", all(p(x,"has_destination") for x in fleet) and
          max(((p(x,"destination")-x.get_actor_location())-(p(fleet[0],"destination")-fleet[0].get_actor_location())).length() for x in fleet)<1)
    pc.action("Stop")
    check("일괄 정지", all(p(x,"hold") for x in fleet))
    pc.action("EvacCharge")
    advance(2)
    check("퇴로 개방 전 충전 차단·정지 시간",not any(p(x,"ftl_charging") for x in fleet) and not p(play,"elapsed_seconds"))
    yield from capture("공습예고_다중선택")
    pc.action("Pause")
    advance(13)
    check("첫 파동 실제 적 항모와 호위",p(play,"arrived_waves")==1 and len(p(pc,"ships"))==11)
    check("적 함재기 실제 출격",any(p(a,"faction_id")==1 and p(a,"state")==state.SORTIE for a in p(carrier,"aircraft")))
    check("적이 있어도 사각형은 아군만 선택", all_friendly()==8 and all(p(x,"friendly") for x in p(pc,"selection")))
    pc.set_target(list(p(pc,"ships"))[-1])
    check("그룹의 같은 표적",all(p(x,"attack_target")==list(p(pc,"ships"))[-1] for x in fleet))
    pc.action("FireAll")
    pc.select_ship(ship)
    pc.action("LaunchAircraft")
    advance(28)
    check("두 번째 파동과 실제 발사",p(play,"arrived_waves")==2 and p(combat,"shot_count")>0)
    check("아군 외부 기체 존재",carrier.has_outstanding_aircraft(ship))
    # Temporarily uncrew the hangars to exercise the hold/abandon choice deterministically.
    systems=p(ship,"systems")
    hangars=[p(d,"slot_id") for d in p(p(systems,"layout"),"slots") if p(d,"facility")==u.OVFacilityType.HANGAR]
    for slot in hangars: systems.debug_set_crew(slot,0)
    all_friendly()
    pc.action("EvacCharge")
    report["charge_at_41"]=[[p(x,"ship_name"),p(x,"selected"),p(x,"ftl_charging"),p(x,"systems").can_command(),p(x,"systems").can_move()] for x in fleet]
    eligible=[x for x in fleet if p(x,"systems").can_command() and p(x,"systems").can_move()]
    check("명령·엔진이 가동되는 선택함 충전",len(eligible)>=5 and ship in eligible and all(p(x,"ftl_charging") == (x in eligible) for x in fleet))
    advance(15)
    check("16초 이전 이탈 금지",p(play,"escaped_ships")==0)
    advance(2)
    report["charge_at_58"]=[[p(x,"ship_name"),combat.is_departed(x),p(x,"ftl_charging"),p(p(x,"systems"),"wrecked")] for x in fleet]
    report["air_at_58"]=[str(p(a,"state")) for a in wing()]
    check("호위함 이탈 후 항모는 회수 대기",p(play,"escaped_ships") >= 4 and all(not combat.is_departed(x) or x in eligible for x in fleet) and not combat.is_departed(ship) and not carrier.can_depart(ship))
    pc.select_ship(ship)
    pc.action("EvacAbandon")
    advance(1)
    if p(play,"result")==outcome.RUNNING: pc.action("EvacFinish")
    check("승리와 실제 충전함 이탈",p(play,"result")==outcome.VICTORY and combat.is_departed(ship) and p(play,"escaped_ships")==sum(combat.is_departed(x) for x in fleet))
    check("포기는 영구 격추와 구분",p(play,"abandoned_aircraft")>0 and ids=={p(a,"aircraft_id") for a in wing()})
    report["victory"]=play.get_status()
    yield from capture("퇴각결과")
    # Normal recovery flight: use only player commands and the registered hangars.
    fleet=start()
    ship=next(x for x in fleet if "항공모함" in p(x,"ship_name"))
    pc.select_ship(ship)
    pc.action("LaunchAircraft")
    pc.action("Pause")
    advance(2)
    check("회수 전 실제 출격",carrier.has_outstanding_aircraft(ship))
    ids={p(a,"aircraft_id") for a in wing()}
    pc.action("RecallAircraft")
    advance(39)
    check("정상 도킹으로 회수 완료",not carrier.has_outstanding_aircraft(ship) and carrier.count_aircraft(ship,state.STORED)>0 and ids=={p(a,"aircraft_id") for a in wing()})
    all_friendly()
    pc.action("EvacCharge")
    advance(18)
    if p(play,"result")==outcome.RUNNING: pc.action("EvacFinish")
    check("일반 명령만으로 함대 승리·항모 이탈",p(play,"result")==outcome.VICTORY and combat.is_departed(ship) and not p(play,"abandoned_aircraft"))
    report["recovery_victory"]=play.get_status()
    for speed in (1,2,3):
        fleet=start()
        pc.action("Speed"+str(speed))
        pc.action("Pause")
        advance(41,speed)
        ship=fleet[-1]
        pc.select_ship(ship)
        pc.action("EvacCharge")
        pc.action("EvacAbandon")
        advance(15,speed)
        check(f"{speed}배속 충전 유지",p(ship,"ftl_charging") and not combat.is_departed(ship))
        advance(2,speed)
        check(f"{speed}배속 개별 이탈",combat.is_departed(ship) and p(play,"escaped_ships")==1)
        pc.action("EvacFinish")
        check(f"{speed}배속 부분 승리",p(play,"result")==outcome.PARTIAL_VICTORY)
    for count in (4,5):
        fleet=start()
        pc.action("Speed1")
        pc.action("Pause")
        advance(41)
        ships=[fleet[i] for i in (7,5,4,3,2)][:count]
        pc.select_ship(ships[0])
        for extra in ships[1:]: pc.select_additional(extra)
        pc.action("EvacCharge")
        advance(18)
        pc.action("EvacFinish")
        check(f"정확히 {count}척 이탈의 승패 경계",p(play,"escaped_ships")==count and p(play,"result")==
              (outcome.VICTORY if count==5 else outcome.PARTIAL_VICTORY))
    fleet=start()
    pc.action("Speed1")
    pc.action("Pause")
    advance(151)
    check("3개 공습 후 퇴로 폐쇄·무이탈 패배",p(play,"arrived_waves")==3 and p(play,"result")==outcome.DEFEAT and not p(play,"escaped_ships"))
    report["defeat"]=play.get_status()
    fleet=start()
    pc.select_ship(fleet[-1])
    pc.action("Pause")
    advance(41)
    pc.action("EvacCharge")
    systems=p(fleet[-1],"systems")
    engines=[p(d,"slot_id") for d in p(p(systems,"layout"),"slots") if p(d,"facility")==u.OVFacilityType.ENGINE]
    for slot in engines: systems.debug_set_crew(slot,0)
    advance(1)
    check("엔진 인원 감소로 충전 취소되지 않음",p(fleet[-1],"ftl_charging"))
    for slot in engines: systems.debug_destroy_slot(slot,u.OVDamageCause.SABOTAGE)
    advance(17)
    check("엔진 전부 파괴 시 도약 취소",not p(fleet[-1],"ftl_charging") and not combat.is_departed(fleet[-1]))
    for x in fleet: p(x,"systems").force_wreck_instant()
    advance(1)
    check("전멸 즉시 패배",p(play,"result")==outcome.DEFEAT)
    pc.action("RestartScene")
    check("재시작 시 파동·이탈·포기 초기화",not p(play,"arrived_waves") and not p(play,"escaped_ships") and not p(play,"abandoned_aircraft") and len(p(pc,"ships"))==8)
    assert pc.start_play_scenario("BlackHoleTrial")
    hazard=play.get_active_black_hole_hazard()
    lens=p(pc,"black_hole_meshes")
    check("원본 블랙홀 표시 배율 80%",abs(lens.get_world_scale().x-1800)<.01 and lens.get_material(0).get_name()=="MI_Blackhole_Physical")
    check("블랙홀 실제 중력권·중심부",p(hazard,"gravity_pull_radius")==100000 and p(hazard,"singularity_radius")==4500)

def tick(delta):
    try:
        next(job)
        return
    except StopIteration:
        report["passed"]=True
    except Exception:
        report.update(passed=False,error=traceback.format_exc())
    (out/"result.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf8")
    u.unregister_slate_post_tick_callback(handle)
    u.OVDevelopmentEditorLibrary.restore_preview_settings()
    u.SystemLibrary.quit_editor()

job=run()
assert u.OVDevelopmentEditorLibrary.start_preview_window(1280,720)
handle=u.register_slate_post_tick_callback(tick)
