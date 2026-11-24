"""격납고 실함 입출고, 승하선, 목표 선택 및 발사 확인의 PIE 검증."""
import json
from pathlib import Path
import traceback
import unreal as u

out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation/ShipOperations.json"
checks = []
result = {"checks": checks, "physicalInput": False}
editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)


def prop(obj, name):
    return obj.get_editor_property(name)


def check(label, value):
    if not value:
        raise AssertionError(label)
    checks.append(label)


def run():
    while not editor.get_game_world():
        yield
    pc = u.GameplayStatics.get_player_controller(editor.get_game_world(), 0)
    while not pc or not pc.get_hud():
        yield
        pc = u.GameplayStatics.get_player_controller(editor.get_game_world(), 0)
    hud = pc.get_hud()
    hud.close_title_screen()
    start = pc.start_play_scenario
    check("영웅 시험장 생성", start("HeroArena"))
    home, enemy = list(prop(pc, "ships"))[:2]
    carrier = prop(pc, "carrier")
    transit = prop(pc, "transport")
    interior = prop(pc, "interior")
    pc.action("ArenaAdd")
    pc.action("ArenaFill")
    def advance(seconds):
        if prop(pc, "simulation_paused"):
            pc.action("Pause")
        for _ in range(round(seconds*20)):
            pc.step_simulation(.05)
    def bay_info(host):
        return [h for h in prop(carrier,"hangars") if prop(h,"carrier")==host]
    def occupants(host):
        return [v for v in prop(interior,"units") if prop(v,"ship")==host and prop(v,"faction_id")==0]
    bay = bay_info(home)[0]
    bid = prop(bay, "hangar_id")
    guest = prop(bay, "stored_small_ship")
    check("실제 보관 강습선", guest and "강습선" in prop(guest,"ship_name"))
    pc.select_ship(home)
    hud.set_details_open(True)
    hud.execute_ui_action("HangarView")
    for _ in range(12):yield
    check("기본 격납고에 강습선 카드",hud.get_action_center("OpsShip_"+str(bid)).x>=0)
    hud.execute_ui_action("OpsShip_"+str(bid))
    check("카드로 동일 함선 상세 열기",prop(pc,"selected")==guest)
    hud.execute_ui_action("OpsHost")
    check("모함으로 복귀",prop(pc,"selected")==home)
    member=next(v for v in occupants(home) if prop(v,"role")!=u.OVInteriorRole.TROOPER)
    uid=prop(member,"unit_id")
    pid=prop(member,"person_id")
    team=[uid]+[prop(v,"unit_id") for v in occupants(home) if prop(v,"role")==u.OVInteriorRole.TROOPER][:4]
    check("영웅과 일반병 함께 승선 지시",pc.transfer_docked_crew(home,guest,team))
    check("승하선 도중 출격 차단",not pc.launch_small_ship(bid))
    check("중복 승선 차단",not pc.transfer_docked_crew(home,guest,team))
    advance(25)
    check("5명 동일 인물로 승선",all(prop(interior.get_unit(i),"ship")==guest for i in team))
    check("개인 승객 ID 보존",prop(transit.get_person(pid),"current_ship")==guest)
    check("모함 하선 명령",pc.transfer_docked_crew(guest,home,team))
    advance(20)
    check("5명 모함 복귀",all(prop(interior.get_unit(i),"ship")==home for i in team))
    check("빈 강습선 독립 출격",pc.launch_small_ship(bid) and prop(pc,"selected")==guest)
    check("같은 격납고 중복 출격 차단",not pc.launch_small_ship(bid))
    check("출격한 강습선 재수납 명령",pc.request_small_ship_dock(bid,guest))
    advance(8)
    check("동일 강습선 복귀",prop(bay_info(home)[0],"stored_small_ship")==guest)
    pc.select_ship(home)
    hud.execute_ui_action("OpsInterior")
    hud.execute_ui_action("InteriorUnit_"+str(uid))
    hud.execute_ui_action("OpsAssault")
    hud.execute_ui_action("OpsPods")
    check("목표 선택 중 아군 클릭 거부",not hud.accept_ship_pick(home))
    check("적함 선택 후 복귀",hud.accept_ship_pick(enemy))
    serial=pc.last_crew_operation()
    check("확인 전 포드 0",len(prop(transit,"pods"))==0)
    advance(25)
    check("집결 후에도 자동 발사 없음",len(prop(transit,"pods"))==0)
    check("발사 전 실제 포드 탑승 대기",prop(interior.get_unit(uid),"embarked_for_departure"))
    check("유효한 발사 확인",pc.confirm_boarding(serial))
    check("중복 확인 차단",not pc.confirm_boarding(serial))
    check("실제 개인 포드 생성",len(prop(transit,"pods"))>0)

    check("다른 대원이 강습선으로 두 번째 팀 준비",pc.prepare_boarding(home,enemy,guest,team[1:]))
    advance(32)
    second=pc.last_crew_operation()
    check("두 번째 팀 강습선 출격 확인",pc.confirm_boarding(second))
    check("원 격납고 비움",not prop(bay_info(home)[0],"stored_small_ship"))
    check("실제 강습선에 일반병 4명 탑승",all(prop(transit.get_person(prop(interior.get_unit(i),"person_id")),"current_ship")==guest for i in team[1:]))
    check("200코스트 함대전 생성",start("Fleet200"))
    ships=list(prop(pc,"ships"))
    allies=[s for s in ships if prop(s,"friendly")]
    for s in ships:
        s.set_automation(False,False)
        s.stop_movement()
    host=next(s for s in allies if "구축전함" in prop(s,"ship_name"))
    empty=[h for h in bay_info(host) if not prop(h,"stored_small_ship")]
    check("무장 구축전함에 설치된 빈 격납고",len(empty)>0)
    bid=prop(empty[0],"hangar_id")
    for hull in ["corvette","repair"]:
        ship=next(s for s in allies if ({"corvette":"초계함","repair":"보급수리함"}[hull] in prop(s,"ship_name")))
        systems=prop(ship,"systems")
        actual_hp=prop(systems,"hull_hp")
        total=len(prop(carrier,"aircraft"))
        people=sorted(prop(v,"unit_id") for v in occupants(ship))
        check(hull+" 접근 명령",pc.request_small_ship_dock(bid,ship))
        check(hull+" 동일 격납고 중복 예약 차단",not pc.request_small_ship_dock(bid,ship))
        for _ in range(120):
            advance(1)
            if any(prop(h,"stored_small_ship")==ship for h in bay_info(host)):break
        result[hull]={"distance":(ship.get_actor_location()-host.get_actor_location()).length(),"message":str(prop(pc,"last_action"))}
        record=next(h for h in bay_info(host) if prop(h,"hangar_id")==bid)
        check(hull+" 빈 격납고 수용",prop(record,"stored_small_ship")==ship)
        check(hull+" 같은 함선 출격·선택",pc.launch_small_ship(bid) and prop(pc,"selected")==ship)
        check(hull+" 실제 승조·함재기 재고 보존",people==sorted(prop(v,"unit_id") for v in occupants(ship)) and len(prop(carrier,"aircraft"))==total)
        check(hull+" 출격으로 무료 수리 없음",prop(systems,"hull_hp")<=actual_hp)
    check("재시작 후 이전 승선 대기 정리",start("HeroArena") and pc.last_crew_operation()==0)


def tick(_):
    try:
        next(job)
        return
    except StopIteration:
        result["passed"]=True
    except Exception:
        result["error"]=traceback.format_exc()
    out.parent.mkdir(exist_ok=True,parents=True)
    out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()


job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
