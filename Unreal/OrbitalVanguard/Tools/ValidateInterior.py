"""C 단계 선내 시스템의 실제 PIE API 검사. OS 입력 검사나 D 통합 임무 완료가 아니다.

명시적 단위 fixture: F7 포드 편성의 DEV 승객을 제거하고 초기 상태를 재설정한 뒤, 기존 피해 API로
쉴드를 한 번 파괴한다. 실제 개인 포드·선내 이동·작전·원 포드 귀환은 생산 경로를 쓴다.
결과: Saved/Validation/Phase3Interior.json. RunValidation.py로 감싸면 종료된다.
"""
import json
import math
import os
import time
import traceback
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
os.makedirs(os.path.join(root, "Validation"), exist_ok=True)
report = {"kind": "C 단계 PIE API·렌더 HUD 검사", "seed": 230117,
          "planning": "035a4fba81d26f1d67daa1f44bded0f7e8d3a434",
          "physical_input": "수행하지 않음", "scope": "단위 fixture이며 D 임무 완료 근거가 아님",
          "checks": [], "errors": [], "clock_samples": []}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {"phase": 0, "at": time.monotonic()}


def p(obj, key):
    return obj.get_editor_property(key)


def check(identifier, passed, detail=""):
    report["checks"].append({"id": identifier, "passed": bool(passed), "detail": str(detail)})
    unreal.log("OVINTERIOR CHECK " + identifier + " = " + str(bool(passed)) + " " + str(detail))
    if not passed:
        raise AssertionError(identifier + ": " + str(detail))


def save():
    report["passed"] = bool(report["checks"]) and all(c["passed"] for c in report["checks"]) and not report["errors"]
    with open(os.path.join(root, "Validation", "Phase3Interior.json"), "w", encoding="utf-8") as out:
        json.dump(report, out, ensure_ascii=False, indent=2)


def pause(pc, value):
    if p(pc, "simulation_paused") != value:
        pc.action("Pause")


def advance(pc, seconds, speed=1):
    pc.action("Speed" + str(speed)); pause(pc, False)
    steps = max(1, math.ceil(seconds / .2))
    for _ in range(steps):
        pc.step_simulation(seconds / steps / speed)
    pause(pc, True)


def slot(ship, kind):
    return next(s for s in p(p(p(ship, "systems"), "layout"), "slots") if p(s, "facility") == kind)


def fixture(pc, heroes=4, troops=2, enter=True):
    start2c = getattr(pc, "start_phase2c_range", None) or getattr(pc, "start_phase2_c_range")
    start2c(3)
    ships = list(p(pc, "ships")); own, escort, target = ships
    combat, transport, interior = p(pc, "combat"), p(pc, "transport"), p(pc, "interior")
    p(pc, "carrier").reset(); p(pc, "point_defense").disable()
    for ship in ships:
        ship.set_automation(False, False); combat.cancel_attack_order(ship); ship.debug_reset_motion()
    own.set_actor_location(unreal.Vector(-10000, 0, 2000), False, False)
    escort.set_actor_location(unreal.Vector(-15000, 10000, 2000), False, False)
    target.set_actor_location(unreal.Vector(10000, 0, 2000), False, False)
    transport.reset_scenario(ships); interior.reset_scenario(ships, 230117)
    # 이 검사의 초기 쉴드 파괴는 외부 무기 교전을 대신하는 명시적 fixture다.
    systems = p(target, "systems")
    # F7의 DEV 쉴드 정지를 제거하고 정상 초기 승무원/쉴드로 재설정한다.
    systems.reset_state()
    systems.apply_world_hit(target.get_actor_location() + target.get_actor_forward_vector() * 100000,
                            target.get_actor_forward_vector(), p(systems, "shield") + 1,
                            unreal.OVDamageCause.EXTERNAL, True)
    ids = list(interior.register_boarding_team(own, 0, heroes, troops))
    if enter:
        launched = transport.launch_pods(own, target)
        if launched != len(ids):
            raise AssertionError("검사 편성의 실제 포드 출발 실패: " + str(launched) + "/" + str(len(ids)) + " " + str(p(transport, "last_message")))
        advance(pc, 4)
    return ships, transport, interior, ids


def core_checks(pc):
    ships, transport, interior, ids = fixture(pc)
    own, _, target = ships
    check("C-ACTUAL-ROSTER", len(ids) == 6 and len(set(ids)) == 6)
    people = list(interior.get_passenger_ids(ids))
    check("C-ONE-ID-TRANSPORT", len(set(people)) == 6 and all(p(transport.get_person(person), "runtime_unit_id") == uid for uid, person in zip(ids, people)))
    check("C-NO-DEV-HERO-CONVERSION", all(not p(transport.get_person(person), "hero_type_dev") for person in people))
    check("C-ACTUAL-POD-ENTRY", all(p(transport.get_person(person), "state") == unreal.OVPassengerState.INSIDE_TARGET for person in people), [str(p(transport.get_person(person), "state")) for person in people])
    check("C-PROTECTION-RUNTIME", p(target, "systems").has_interior_presence(0) and not p(target, "systems").is_interior_developer_input(0))
    check("C-STRUCTURE-AFTER-ENTRY", interior.is_structure_known(0, target))
    check("C-DEV-RETURN-REJECTED", not transport.dev_return_person_to_entry(people[0]))
    person = transport.get_person(people[0]); entry = p(person, "entry_local_point")
    check("C-FALSE-ENTRY-POSITION-REJECTED", not transport.return_runtime_person_to_entry(people[0], ids[0], target, entry + unreal.Vector(10000, 0, 0), p(person, "entry_layout_version")))
    check("C-FORGED-ENTRY-RETURN-REJECTED", not transport.return_runtime_person_to_entry(people[0], ids[0], target, entry, p(person, "entry_layout_version")))
    shield = slot(target, unreal.OVFacilityType.SHIELD_CONTROL); shield_id = p(shield, "slot_id")
    before = p(target, "systems").get_slot_state(shield_id)
    interior.issue_order(ids, unreal.OVInteriorOrder.CAPTURE, shield_id)
    advance(pc, .1)
    check("C-NO-INSTANT-CAPTURE", p(p(target, "systems").get_slot_state(shield_id), "occupying_faction") != 0)
    advance(pc, 65)
    after = p(target, "systems").get_slot_state(shield_id)
    check("C-REAL-MOVE-AND-CAPTURE", p(after, "occupying_faction") == 0 and p(interior.get_unit(ids[0]), "distance_travelled") > 100)
    check("C-SHIELD-EXTERNAL-EFFECT", p(p(target, "systems"), "max_shield") == 0 and not p(p(target, "systems"), "shield_active"))
    check("C-PHYSICAL-CREW-OCCUPATION-SEPARATE", p(after, "hp") == p(before, "hp") and p(after, "crew") == p(before, "crew"))
    check("C-OBJECTIVE-ACTUAL-RESULT", p(target, "systems").is_interior_objective_complete(0))
    bridge_id = p(slot(target, unreal.OVFacilityType.BRIDGE), "slot_id")
    interior.issue_order(ids, unreal.OVInteriorOrder.CAPTURE, bridge_id)
    check("C-CHANGED-OBJECTIVE-CLEARS-RESULT", not p(target, "systems").is_interior_objective_complete(0))
    pods = [p(transport.get_person(person), "pod_id") for person in people]
    hp = [p(interior.get_unit(uid), "hp") for uid in ids]
    interior.issue_order(ids, unreal.OVInteriorOrder.RETREAT)
    advance(pc, 50)
    check("C-REAL-EXTRACTION", all(p(transport.get_person(person), "state") == unreal.OVPassengerState.ABOARD for person in people), [str(p(transport.get_person(person), "state")) for person in people])
    check("C-ORIGINAL-PODS", pods == [p(transport.get_person(person), "pod_id") for person in people] and len(p(transport, "pods")) == 6)
    check("C-NO-FREE-HEAL-RETURN", hp == [p(interior.get_unit(uid), "hp") for uid in ids])
    check("C-PROTECTION-LAST-EXIT", not p(target, "systems").has_interior_presence(0))
    check("C-NO-GHOST-PRESENCE", p(interior, "total_deaths") == 0)

    # 동일 편성/시드, 동일 게임 시간만 비교한다. API 벽시계 성능 검사가 아니다.
    for speed in (1, 2, 3):
        ships, transport, interior, ids = fixture(pc, 2, 2)
        target = ships[2]; shield_id = p(slot(target, unreal.OVFacilityType.SHIELD_CONTROL), "slot_id")
        interior.issue_order(ids, unreal.OVInteriorOrder.CAPTURE, shield_id)
        at = p(interior, "game_time"); pc.step_simulation(.5)
        check("C-PAUSE-" + str(speed), p(interior, "game_time") == at)
        advance(pc, 30, speed)
        report["clock_samples"].append({"speed": speed, "game_time": round(p(interior, "game_time"), 3),
            "units": [(str(p(interior.get_unit(uid), "state")), round(p(interior.get_unit(uid), "progress"), 3), round(p(interior.get_unit(uid), "distance_travelled"), 1)) for uid in ids],
            "captures": p(interior, "total_captures")})
    cases = [{k:v for k,v in sample.items() if k != "speed"} for sample in report["clock_samples"]]
    check("C-CLOCK-123", cases[0] == cases[1] == cases[2], cases)

    # 실제 유닛의 외부 운송 손실은 DEV형 토큰과 분리해 같은 사망/구조 경계를 사용한다.
    ships, transport, interior, ids = fixture(pc, 1, 1, False)
    settings = p(transport, "settings"); settings.set_editor_property("pod_speed", 100)
    transport.set_editor_property("settings", settings)
    transport.launch_pods(ships[0], ships[2]); advance(pc, .5)
    for flight in p(transport, "pods"):
        transport.intercept_pod(p(flight, "pod_id"), 100000)
    advance(pc, .1)
    check("C-EXTERNAL-HERO-RESCUE", p(interior.get_unit(ids[0]), "state") == unreal.OVInteriorState.RESCUE_WAITING and interior.count_active_heroes(0) == 1)
    check("C-EXTERNAL-TROOP-DEATH", p(interior.get_unit(ids[1]), "state") == unreal.OVInteriorState.DEAD)
    deaths = p(interior, "total_deaths")
    for flight in p(transport, "pods"):
        transport.intercept_pod(p(flight, "pod_id"), 100000)
    advance(pc, .2)
    check("C-DUPLICATE-CASUALTY", p(interior, "total_deaths") == deaths)
    settings.set_editor_property("pod_speed", 16500); transport.set_editor_property("settings", settings)

    # HUD 검사는 실제 입력 대신 안정적인 ID와 동일 명령 경로를 사용한다.
    ships, transport, interior, ids = fixture(pc, 4, 2)
    target = ships[2]; pc.select_ship(target)
    hud = pc.get_hud(); hud.set_details_open(True)
    if not p(hud, "interior_open"):
        hud.handle_interior_action("InteriorToggle")
    else:
        hud.handle_interior_action("InteriorSelectedShip")
    shield = slot(target, unreal.OVFacilityType.SHIELD_CONTROL)
    hud.handle_interior_action("InteriorDeck_" + str(p(shield, "deck_id")))
    hud.handle_interior_action("InteriorAll")
    hud.handle_interior_action("InteriorSlot_" + str(p(shield, "slot_id")))
    state.update(ships=ships, interior=interior, transport=transport, ids=ids, shield_id=p(shield, "slot_id"))


def tick(dt):
    try:
        if state["phase"] == 0:
            level.editor_request_begin_play(); state["phase"] = 1; return
        world = editor.get_game_world()
        if not world: return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or len(p(pc, "ships")) < 3: return
        if state["phase"] == 1:
            core_checks(pc); state.update(phase=2, at=time.monotonic()); return
        if time.monotonic()-state["at"] < .5: return
        if state["phase"] == 2:
            hud = pc.get_hud(); center = hud.get_action_center("InteriorCapture")
            check("C-HUD-STABLE-ID", center.x >= 0 and str(hud.get_hit_action_at_canvas_point(center)) == "InteriorCapture")
            width, height = pc.get_viewport_size(); scale = max(.25, min(1, width/1280, height/740))
            check("C-HUD-CONSUMES", hud.is_over_interior_panel(center.x*scale, center.y*scale))
            check("C-HUD-ACTUAL-SELECTION", set(p(hud, "interior_selected_units")) == set(state["ids"]))
            check("C-HUD-CAPTURE-ACCEPTED", hud.handle_interior_action("InteriorCapture"))
            advance(pc, 65)
            check("C-HUD-COMMAND-REAL-RESULT", p(p(state["ships"][2], "systems").get_slot_state(state["shield_id"]), "occupying_faction") == 0)
            hud.handle_interior_action("InteriorRetreat"); advance(pc, 50)
            check("C-HUD-RETREAT-REAL-RESULT", all(p(state["transport"].get_person(person), "state") == unreal.OVPassengerState.ABOARD for person in state["interior"].get_passenger_ids(state["ids"])))
            hud.handle_interior_action("InteriorClose"); hud.set_details_open(False)
            pc.start_combat_range()
            check("C-F5-RESET-DISABLE", not p(p(pc, "interior"), "enabled"))
            pc.start_calibration_range(1)
            check("C-F6-RESET-DISABLE", not p(p(pc, "interior"), "enabled"))
            start2c = getattr(pc, "start_phase2c_range", None) or getattr(pc, "start_phase2_c_range")
            start2c(5)
            check("C-F7-DEV-PRESERVED", not p(p(pc, "interior"), "enabled") and len(p(p(pc, "transport"), "persons")) == 10)
            save(); unreal.unregister_slate_post_tick_callback(state["handle"])
            unreal.log("OVINTERIOR VALIDATE COMPLETE")
    except Exception:
        report["errors"].append(traceback.format_exc()); save()
        unreal.log_error(report["errors"][-1])
        unreal.unregister_slate_post_tick_callback(state["handle"])


state["handle"] = unreal.register_slate_post_tick_callback(tick)
