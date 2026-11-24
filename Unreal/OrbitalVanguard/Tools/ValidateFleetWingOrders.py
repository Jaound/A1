"""200코스트 강습·항공대 명령과 함선 화면을 실제 PIE에서 확인한다."""
from pathlib import Path
import json
import traceback
import unreal as u

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
folder = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation/FleetWingOrders"
folder.mkdir(parents=True, exist_ok=True)
report = {"checks": []}

def get(obj, key):
    return obj.get_editor_property(key)

def check(label, value):
    if not value:
        raise AssertionError(label)
    report["checks"].append(label)

def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = u.GameplayStatics.get_player_controller(world, 0)
    while not pc or not pc.get_hud():
        yield
        pc = u.GameplayStatics.get_player_controller(world, 0)
    hud = pc.get_hud()
    hud.close_title_screen()
    u.SystemLibrary.execute_console_command(world, "ov.Trial.Fleet200Shuttle 1", pc)
    def setup():
        check("양측 200코스트 생성", pc.start_play_scenario("Fleet200") and list(get(pc.get_play_scenario(), "initial_costs")) == [200,200])
        ships = list(get(pc, "ships"))
        for ship in ships:
            ship.set_automation(False, False)
            ship.stop_movement()
        if not get(pc, "simulation_paused"):
            pc.action("Pause")
        return ships
    def advance(secs):
        if get(pc, "simulation_paused"):
            pc.action("Pause")
        for _ in range(round(secs * 20)):
            pc.step_simulation(.05)
        if not get(pc, "simulation_paused"):
            pc.action("Pause")
    def find(text, side=True):
        return next(s for s in ships if get(s,"friendly")==side and text in get(s,"ship_name"))
    def facility(ship, kind):
        layout=get(get(ship,"systems"),"layout")
        return next(room for room in get(layout,"slots") if get(room,"facility")==kind)
    ships=setup()
    host, enemy, guest, vector = find("구축전함"), find("구축전함",False), find("강습선"), find("백터")
    for ship in ships:
        room=facility(ship,u.OVFacilityType.BRIDGE)
        check(get(ship,"ship_name")+" 함교 후방",get(room,"local_center").x<0)
        layout = get(get(ship,"systems"),"layout")
        engines = [s for s in get(layout,"slots") if get(s,"facility")==u.OVFacilityType.ENGINE]
        stern = min(get(s,"local_center").x for s in get(layout,"slots"))
        check(get(ship,"ship_name")+" 엔진 선미 고정", engines and all(
            abs(get(s,"local_center").x-stern)<.01 and
            list(get(s,"allowed_facilities"))==[u.OVFacilityType.ENGINE] for s in engines))
        check(get(ship,"ship_name")+" 함교와 엔진 분리", get(room,"local_center").x>stern)
    gs,vs=get(guest,"systems"),get(vector,"systems")
    check("백터급 선체, 강습선 고유 쉴드",get(gs,"max_hull_hp")==get(vs,"max_hull_hp")==21252 and get(gs,"max_shield")==6336)
    def armor(systems):
        return {get(a,"face").name:get(a,"max_armor") for a in get(get(systems,"layout"),"armor_sections")}
    check("방향별 장갑 동일",armor(gs)==armor(vs))
    pc.select_ship(host)
    hud.set_details_open(True)
    yield
    pc.select_ship(vector)
    yield
    hud.set_details_open(False)
    pc.select_ship(host)
    host.set_actor_location(enemy.get_actor_location()+u.Vector(-17500,0,0),False,False)
    units,transit=get(pc,"interior"),get(pc,"transport")
    troops=list(units.register_boarding_team(host,0,0,4))
    heroes=[get(v,"unit_id") for v in get(units,"units") if get(v,"ship")==host and get(v,"role")!=u.OVInteriorRole.TROOPER]
    team=[troops[0], heroes[1], heroes[0], troops[1]]
    check("포드 순서 혼합 편성",pc.prepare_boarding(host,enemy,None,team))
    serial=pc.last_crew_operation()
    advance(10)
    for _ in range(4):
        yield
    action="OpsReady_"+str(serial)
    check("집결 후 실제 확인 버튼",hud.get_action_center(action).x>0)
    hud.execute_ui_action(action)
    pods=sorted(get(transit,"pods"), key=lambda pod:get(pod,"scheduled_time"))
    check("화면 편성 순서대로 개인 포드 발사 예약",[get(pod,"person_id") for pod in pods]==[get(units.get_unit(i),"person_id") for i in team])
    check("포드 기존 내구도 160", all(get(pod,"hp")==160 for pod in pods))
    ships=setup()
    host,enemy,guest=find("구축전함"),find("구축전함",False),find("강습선")
    host.set_actor_location(enemy.get_actor_location()+u.Vector(-17500,0,0),False,False)
    crew=list(units.register_boarding_team(host,0,0,4))
    check("강습선 승선",pc.prepare_boarding(host,enemy,guest,crew))
    advance(10)
    check("강습선 출격",pc.confirm_boarding(pc.last_crew_operation()))
    def shuttle():
        return next(s for s in get(transit,"shuttles") if get(s,"ship")==guest)
    advance(15)
    check("활성 쉴드에 접안 보류",get(shuttle(),"dock_blocked") and "쉴드" in get(shuttle(),"reason"))
    gs=get(guest,"systems")
    bridge=facility(guest,u.OVFacilityType.BRIDGE)
    slot=get(bridge,"slot_id")
    gs.debug_set_crew(slot,0)
    advance(.2)
    check("함교 무력화 표시",get(shuttle(),"dock_blocked") and "함교" in get(shuttle(),"reason"))
    yield
    gs.debug_set_crew(slot,get(bridge,"required_crew"))
    get(enemy,"systems").debug_set_crew(get(facility(enemy,u.OVFacilityType.SHIELD_CONTROL),"slot_id"),0)
    advance(12)
    check("함교 복구·쉴드 해제 후 실제 이송",not get(shuttle(),"dock_blocked") and get(shuttle(),"state")==u.OVShuttleState.DELIVERED)
    ships=setup()
    own,red=find("항공모함"),find("항공모함",False)
    red.set_actor_location(own.get_actor_location()+u.Vector(22000,0,0),False,False)
    wing=get(pc,"carrier")
    pc.select_ship(own)
    for _ in range(4):
        yield
    air_button = hud.get_action_center("WingAirCombat")
    jump_button = hud.get_action_center("PlotJump")
    check("제공전과 초광속 버튼 분리", air_button.x > 0 and jump_button.x > 0 and air_button.y > jump_button.y + 27)
    hud.execute_ui_action("WingAirCombat")
    def wings(): return [a for a in get(wing,"aircraft") if get(a,"carrier")==own]
    def state(): return next(c for c in get(wing,"carriers") if get(c,"carrier")==own)
    check("제공전 설정 후 명시 출격 대기",get(state(),"air_superiority") and all(get(a,"state")==u.OVAircraftState.STORED for a in wings()))
    hud.execute_ui_action("LaunchAircraft")
    wing.launch(red)
    dogfight=False
    hit=False
    for _ in range(24):
        advance(.5)
        dogfight |= any(get(a,"maneuver")==u.OVAircraftManeuver.DOGFIGHT for a in wings())
        hit |= any(get(b,"faction_id")==0 and str(get(b,"hit_id")).startswith("Aircraft_") for b in get(wing,"recent_events"))
        if dogfight and hit:break
    check("적 함재기 추격과 실제 기체 명중",dogfight and hit)
    check("제공전 중 대함 표적 없음",all(not get(a,"strike_target") for a in wings()))
    yield
    hud.execute_ui_action("WingAirCombat")
    check("대함 임무 복귀",not get(state(),"air_superiority"))
    hud.execute_ui_action("RecallAircraft")
    advance(.1)
    check("명시 귀환 우선",all(get(a,"state")!=u.OVAircraftState.SORTIE for a in wings()))

    # 두 원본 메시가 모두 날아가는 사거리 시험장을 사용한다.
    check("미사일 실사격 시험장", pc.start_calibration_range(2))
    combat = get(pc, "combat")
    ships = list(get(pc, "ships"))
    pc.set_editor_property("developer_mode", True)
    cruiser, = get(pc, "development").spawn_ships("cruiser", 0, 1, u.Vector(0,8000,2500), 0, 0, 0, False, True)
    pc.set_editor_property("developer_mode", False)
    for shooter, target in [(ships[0],ships[2]), (ships[1],ships[2]), (cruiser,ships[2]), (ships[2],cruiser)]:
        shooter.set_automation(False, True)
        combat.set_target(shooter, target)
    if get(pc, "simulation_paused"):
        pc.action("Pause")
    measured = {}
    for step in range(900):
        pc.step_simulation(.05)
        pc.update_combat_presentation()
        for prop in ("standard_missile_meshes", "anti_ship_missile_meshes"):
            comp = get(pc, prop)
            if comp.get_instance_count():
                measured[prop] = comp.get_instance_transform(0,True).scale3d.x
        if len(measured) == 2:
            break
        if step % 30 == 0:
            yield
    report["missile_scales"] = measured
    for prop, previous in [("standard_missile_meshes",.195), ("anti_ship_missile_meshes",.45)]:
        check(prop+" 비행 메시 1.5배", abs(measured.get(prop,0)-previous*1.5)<.0001)

def tick(_):
    try:
        next(job)
        return
    except StopIteration:
        report["passed"]=True
    except Exception:
        report["error"]=traceback.format_exc()
    (folder/"Results.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()

job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
