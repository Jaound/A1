"""F6 기존 적함에 명시 편성 영웅이 진입·이동·원 포드 귀환하는 PIE API 검사."""
import json
import os
import traceback
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
os.makedirs(os.path.join(root, "Validation"), exist_ok=True)
report = {"kind": "F6 보유 영웅 강습 PIE API", "checks": [], "errors": [],
          "scope": "명시적 DEV 시험 함선과 쉴드 파괴 fixture; 실제 키보드·마우스 검사는 아님"}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {"phase": 0}


def p(obj, key):
    return obj.get_editor_property(key)


def check(name, value, detail=""):
    report["checks"].append({"id": name, "passed": bool(value), "detail": str(detail)})
    unreal.log("OVHEROBOARD " + name + " = " + str(bool(value)) + " " + str(detail))
    if not value:
        raise AssertionError(name + ": " + str(detail))


def advance(interior, transport, seconds):
    for _ in range(int(seconds * 10)):
        transport.simulate(.1)
        interior.simulate(.1)


def run(pc):
    dev, heroes = p(pc, "development"), p(pc, "heroes")
    check("HB-ROSTER", dev.open_roster_room(), p(dev, "last_message"))
    hero = heroes.create_dev_hero(0, unreal.OVInteriorRole.COMMANDER, "강습 연결 검사 영웅")
    check("HB-RESERVE", heroes.set_deployment(hero, True), p(heroes, "last_message"))
    check("HB-F6", pc.start_calibration_range(1))
    if not p(pc, "developer_mode"):
        pc.action("Developer")
    original_ships = list(p(pc, "ships")); target = original_ships[2]
    target_pos = target.get_actor_location()
    # 지휘 보직과 포드 발사 시설이 공존하는 항모를 사용한다. 강습선은 수송 영웅 보직 금지.
    spawned = list(dev.spawn_ships("carrier", 0, 1, target_pos + unreal.Vector(-15000, 0, 0), 0, 0, 0, False, False))
    check("HB-EXPLICIT-TEST-ORIGIN", len(spawned) == 1, p(dev, "last_message"))
    origin = spawned[0]; interior = p(pc, "interior"); transport = p(pc, "transport")
    check("HB-TARGET-NOT-KNOWN-BEFORE-ENTRY", not interior.is_structure_known(0, target))
    unit = interior.spawn_roster_hero(origin, hero, "")
    check("HB-SPAWN", unit > 0, p(interior, "last_message"))
    person_id = p(interior.get_unit(unit), "person_id")
    check("HB-NOT-AUTOMATICALLY-ASSIGNED", not p(transport.get_person(person_id), "boarding_assigned"))
    check("HB-UNASSIGNED-NO-LAUNCH", transport.launch_pods(origin, target) == 0)
    bridge = next(s for s in p(p(p(origin, "systems"), "layout"), "slots") if p(s, "facility") == unreal.OVFacilityType.BRIDGE)
    bridge_id = p(bridge, "slot_id")
    check("HB-DUTY-REQUEST", interior.set_hero_duty(unit, True, bridge_id))
    check("HB-DUTY-NOT-AUTO-DRAFTED", not interior.set_hero_boarding_assignment(unit, True))
    check("HB-DUTY-RELEASE", interior.set_hero_duty(unit, False, ""))
    check("HB-EXPLICIT-ASSIGN", interior.set_hero_boarding_assignment(unit, True), p(interior, "last_message"))
    check("HB-PERSON-ASSIGNMENT", p(transport.get_person(person_id), "boarding_assigned"))
    # 시험 fixture: 실제 피해 API로 보호막을 꺼서 진입 경계를 재현한다.
    systems = p(target, "systems")
    systems.apply_world_hit(target.get_actor_location() + target.get_actor_forward_vector() * 100000,
                            target.get_actor_forward_vector(), p(systems, "shield") + 1,
                            unreal.OVDamageCause.EXTERNAL, True)
    check("HB-SHIELD-DOWN", not p(systems, "shield_active"))
    check("HB-QUEUE-ONE", transport.launch_pods(origin, target) == 1, p(transport, "last_message"))
    queued = list(p(transport, "pods")); check("HB-QUEUED-POD", len(queued) == 1)
    original_pod_id = p(queued[0], "pod_id")
    check("HB-UNASSIGN-BEFORE-LAUNCH", interior.set_hero_boarding_assignment(unit, False))
    advance(interior, transport, .1)
    cancelled = next(pod for pod in p(transport, "pods") if p(pod, "pod_id") == original_pod_id)
    check("HB-QUEUED-CANCELLED-PERSON-ABOARD", p(cancelled, "state") == unreal.OVPodFlightState.CANCELLED
          and p(transport.get_person(person_id), "state") == unreal.OVPassengerState.ABOARD)
    check("HB-REASSIGN", interior.set_hero_boarding_assignment(unit, True))
    check("HB-SAME-TARGET-INTERVAL", transport.launch_pods(origin, target) == 0)
    advance(interior, transport, 30.0)
    check("HB-REUSE-CANCELLED-POD", transport.launch_pods(origin, target) == 1 and len(p(transport, "pods")) == 1
          and p(list(p(transport, "pods"))[0], "pod_id") == original_pod_id)
    advance(interior, transport, .1)
    check("HB-FLYING-NOT-CANCELLED", p(transport.get_person(person_id), "state") == unreal.OVPassengerState.IN_POD
          and not interior.set_hero_boarding_assignment(unit, False))
    advance(interior, transport, 5)
    entered = interior.get_unit(unit)
    check("HB-ACTUAL-ENTRY", p(transport.get_person(person_id), "state") == unreal.OVPassengerState.INSIDE_TARGET,
          p(interior.get_unit(unit), "reason"))
    check("HB-GRAPH-REVEALED-ON-ENTRY", interior.is_structure_known(0, target))
    entry_slot = p(entered, "node_id")
    slots = p(p(systems, "layout"), "slots")
    captures_before = p(interior, "total_captures")
    advance(interior, transport, 20)
    check("HB-DEFEND-DOES-NOT-CAPTURE", p(interior, "total_captures") == captures_before
          and all(p(op, "progress") == 0 for op in p(interior, "operations") if p(op, "ship") == target and p(op, "faction_id") == 0))
    destination = next(p(s, "slot_id") for s in slots if p(s, "facility") == unreal.OVFacilityType.SHIELD_CONTROL)
    check("HB-ORDER-ACCEPTED", interior.issue_order([unit], unreal.OVInteriorOrder.CAPTURE, destination))
    progress = 0.0
    for _ in range(240):
        advance(interior, transport, .5)
        matching = [op for op in p(interior, "operations") if p(op, "ship") == target and p(op, "slot_id") == destination and p(op, "faction_id") == 0]
        progress = p(matching[0], "progress") if matching else 0
        if progress > 0:
            break
    check("HB-EXPLICIT-CAPTURE-PROGRESSES", 0 < progress < 1, progress)
    check("HB-ACTUAL-MOVEMENT", p(interior.get_unit(unit), "distance_travelled") > 10, p(interior.get_unit(unit), "reason"))
    check("HB-DEFEND-ORDER", interior.issue_order([unit], unreal.OVInteriorOrder.DEFEND, destination))
    advance(interior, transport, 2)
    matching = [op for op in p(interior, "operations") if p(op, "ship") == target and p(op, "slot_id") == destination and p(op, "faction_id") == 0]
    check("HB-DEFEND-STOPS-CAPTURE", len(matching) == 1 and p(matching[0], "progress") <= progress
          and p(interior, "total_captures") == captures_before, p(matching[0], "progress") if matching else "no operation")
    check("HB-RETREAT-ORDER", interior.issue_order([unit], unreal.OVInteriorOrder.RETREAT, ""))
    advance(interior, transport, 90)
    check("HB-ORIGINAL-POD-RECOVERY", p(transport.get_person(person_id), "state") == unreal.OVPassengerState.ABOARD
          and p(transport.get_person(person_id), "current_ship") == origin, p(interior.get_unit(unit), "reason"))
    check("HB-SAME-PERSON-AND-UNIT", p(interior.get_unit(unit), "person_id") == person_id and p(transport.get_person(person_id), "runtime_unit_id") == unit)
    check("HB-ENEMY-PRESENCE-CLEARED", not systems.has_interior_presence(0))


def tick(_dt):
    try:
        if state["phase"] == 0:
            level.editor_request_begin_play(); state["phase"] = 1; return
        world = editor.get_game_world()
        if not world:
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not p(pc, "heroes") or not p(pc, "development"):
            return
        run(pc)
    except Exception:
        report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1])
    report["passed"] = bool(report["checks"]) and all(c["passed"] for c in report["checks"]) and not report["errors"]
    with open(os.path.join(root, "Validation", "HeroBoarding.json"), "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state["handle"])


state["handle"] = unreal.register_slate_post_tick_callback(tick)
