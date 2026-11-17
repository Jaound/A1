"""실제 PIE GameInstance 영웅 명단과 선내 보직 API 검사. 화면 입력 검사는 별도다."""
import json
import os
import time
import traceback
import uuid
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
os.makedirs(os.path.join(root, "Validation"), exist_ok=True)
report = {"kind": "영웅 명단·선내 보직 PIE API 검사", "checks": [], "errors": [],
          "scope": "D114 v0.5 후보; 실제 키보드/마우스 및 장기 밸런스 검사 아님"}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {"phase": 0, "slot": "OVHeroValidation_" + uuid.uuid4().hex}


def p(obj, key):
    return obj.get_editor_property(key)


def guid_key(value):
    return tuple(p(value, key) for key in ("a", "b", "c", "d"))


def check(name, condition, detail=""):
    report["checks"].append({"id": name, "passed": bool(condition), "detail": str(detail)})
    unreal.log("OVHERO " + name + " = " + str(bool(condition)) + " " + str(detail))
    if not condition:
        raise AssertionError(name + ": " + str(detail))


def finish():
    unreal.GameplayStatics.delete_game_in_slot(state["slot"], 0)
    report["passed"] = bool(report["checks"]) and all(c["passed"] for c in report["checks"]) and not report["errors"]
    with open(os.path.join(root, "Validation", "HeroRoster.json"), "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)


def run(pc):
    dev, heroes = p(pc, "development"), p(pc, "heroes")
    check("H-ROSTER-ROOM", dev.open_roster_room(), p(dev, "last_message"))
    check("H-ACTUAL-GAMEINSTANCE-SERVICE", heroes is not None)
    role = unreal.OVInteriorRole.COMMANDER
    ids = [heroes.create_dev_hero(0, role, "검사 영웅 %d" % i) for i in range(16)]
    records = list(heroes.get_heroes(0))
    check("H-UNIQUE-16-PEOPLE", len(records) == 16 and len({guid_key(p(h, "hero_id")) for h in records}) == 16)
    for hero_id in ids[:15]:
        check("H-DEPLOY-" + str(len(report["checks"])), heroes.set_deployment(hero_id, True), p(heroes, "last_message"))
    check("H-FACTION-15-LIMIT", heroes.count_deployed(0) == 15 and not heroes.set_deployment(ids[15], True))
    check("H-DEV-XP", heroes.grant_dev_xp(ids[0], 200000), p(heroes, "last_message"))
    grown = heroes.get_hero(ids[0])
    check("H-LEVEL-SKILLS", p(grown, "level") == 20 and p(grown, "skill_points") == 6 and list(p(grown, "skill_ranks")) == [1, 1, 1, 1])
    check("H-INVEST", heroes.spend_skill_point(ids[0], 0) and list(p(heroes.get_hero(ids[0]), "skill_ranks")) == [2, 1, 1, 1])
    check("H-SAVE", heroes.save_roster(state["slot"], 0), p(heroes, "last_message"))
    check("H-MUTATE-AFTER-SAVE", heroes.spend_skill_point(ids[0], 0))
    check("H-LOAD", heroes.load_roster(state["slot"], 0), p(heroes, "last_message"))
    loaded = heroes.get_hero(ids[0])
    loaded_id, expected_id = guid_key(p(loaded, "hero_id")), guid_key(ids[0])
    loaded_ranks, loaded_points = list(p(loaded, "skill_ranks")), p(loaded, "skill_points")
    check("H-SAVE-ID-AND-SKILLS", loaded_id == expected_id and loaded_ranks == [2, 1, 1, 1] and loaded_points == 5,
          "loaded_id=%s expected_id=%s ranks=%s points=%s" % (loaded_id, expected_id, loaded_ranks, loaded_points))
    for hero_id in ids[1:15]:
        check("H-RELEASE-" + str(len(report["checks"])), heroes.set_deployment(hero_id, False))
    # 영웅창의 "선택 함선에 배치"와 동일한 API 경로를 F6 초기 세 척에서 검증한다.
    # UI 물리 클릭은 별도 검사다. 장면 전환 전에 두 명을 편성 예약한다.
    check("H-F6-SECOND-RESERVE", heroes.set_deployment(ids[1], True))
    # F6 0번은 두 번째 아군을 이탈 처리하므로 실제 세 척을 쓰는 1번 장면을 선택한다.
    check("H-F6-START", pc.start_calibration_range(1))
    f6ships = list(p(pc, "ships")); f6interior = p(pc, "interior"); f6transport = p(pc, "transport")
    check("H-F6-INITIAL-NO-INTERIOR", not p(f6interior, "enabled"))
    pc.select_ship(f6ships[0])
    first_id = f6interior.spawn_roster_hero(p(pc, "selected"), ids[0], "")
    check("H-F6-SELECTED-SHIP-SPAWN", first_id > 0, p(f6interior, "last_message"))
    first_unit = f6interior.get_unit(first_id)
    first_person = p(first_unit, "person_id")
    check("H-F6-TRANSPORT-LINK", first_person > 0 and p(f6transport.get_person(first_person), "runtime_unit_id") == first_id)
    pc.select_ship(f6ships[1])
    second_id = f6interior.spawn_roster_hero(p(pc, "selected"), ids[1], "")
    check("H-F6-SECOND-SHIP-PRESERVES-FIRST", second_id > 0 and p(f6interior.get_unit(first_id), "person_id") == first_person
          and p(f6transport.get_person(first_person), "runtime_unit_id") == first_id)
    f6bridge = next(s for s in p(p(p(f6ships[0], "systems"), "layout"), "slots") if p(s, "facility") == unreal.OVFacilityType.BRIDGE)
    f6bridge_id = p(f6bridge, "slot_id")
    check("H-F6-DUTY-COMMAND", f6interior.set_hero_duty(first_id, True, f6bridge_id), p(f6interior, "last_message"))
    f6interior.simulate(60.0)
    check("H-F6-DUTY-ACTUAL-ARRIVAL", p(f6interior.get_unit(first_id), "duty_active") and p(f6interior.get_duty_bonuses(f6ships[0]), "speed_bonus") > 0,
          p(f6interior.get_unit(first_id), "reason"))
    check("H-F6-DUTY-RELEASE", f6interior.set_hero_duty(first_id, False, f6bridge_id) and p(f6interior.get_duty_bonuses(f6ships[0]), "speed_bonus") == 0)
    check("H-F6-ROOM-RETURN", dev.open_roster_room(), p(dev, "last_message"))
    check("H-F6-SECOND-RELEASE", heroes.set_deployment(ids[1], False))
    start2c = getattr(pc, "start_phase2c_range", None) or getattr(pc, "start_phase2_c_range")
    start2c(3)
    fixture_ships = list(p(pc, "ships"))
    fixture_transport, fixture_interior = p(pc, "transport"), p(pc, "interior")
    fixture_transport.reset_scenario(fixture_ships)
    fixture_interior.reset_scenario(fixture_ships, 260112)
    check("H-DIRECT-ENEMY-SPAWN-REFUSED", fixture_interior.spawn_roster_hero(fixture_ships[2], ids[0], "") == 0)
    check("H-OWN-SHIP-SPAWN-ALLOWED", fixture_interior.spawn_roster_hero(fixture_ships[0], ids[0], "") > 0, p(fixture_interior, "last_message"))
    check("H-RETURN-TO-ROSTER-ROOM", dev.open_roster_room(), p(dev, "last_message"))
    mission = p(pc, "mission")
    check("H-OWNED-MISSION", mission.start_roster_mission(pc, False, 260112, False), p(mission, "status"))
    interior = p(pc, "interior")
    own = p(mission, "origin")
    bound = heroes.get_hero(ids[0]); uid = p(bound, "runtime_unit_id"); person = p(bound, "runtime_person_id")
    check("H-RUNTIME-IDS", uid > 0 and person > 0 and guid_key(p(interior.get_unit(uid), "hero_id")) == guid_key(ids[0]))
    mission.simulate(1.0)
    check("H-IDLE-NO-PARTICIPATION-XP", mission.get_hero_participation_seconds(ids[0]) == 0 and mission.get_hero_objective_xp(ids[0]) == 0)
    check("H-NO-DUPLICATE-RUNTIME", interior.spawn_roster_hero(own, ids[0], "") == 0)
    check("H-NO-SAVE-IN-COMBAT", not heroes.save_roster(state["slot"], 0))
    bridge = next(s for s in p(p(p(own, "systems"), "layout"), "slots") if p(s, "facility") == unreal.OVFacilityType.BRIDGE)
    bridge_id = p(bridge, "slot_id")
    check("H-DUTY-ACCEPT", interior.set_hero_duty(uid, True, bridge_id), p(interior, "last_message"))
    check("H-DUTY-NOT-INSTANT", not p(interior.get_unit(uid), "duty_active") and p(interior.get_duty_bonuses(own), "speed_bonus") == 0)
    interior.simulate(30.0)
    unit = interior.get_unit(uid)
    check("H-DUTY-ACTUAL-ARRIVAL", p(unit, "duty_active") and p(unit, "node_id") == bridge_id, p(unit, "reason"))
    check("H-DUTY-APPLIED", p(interior.get_duty_bonuses(own), "speed_bonus") > 0)
    ready = p(unit, "skill3_ready_at")
    check("H-DUTY-L10-COOLDOWN", ready > p(interior, "game_time"))
    check("H-DUTY-RELEASE", interior.set_hero_duty(uid, False, bridge_id))
    after = interior.get_unit(uid)
    check("H-DUTY-EFFECT-REMOVED", p(interior.get_duty_bonuses(own), "speed_bonus") == 0 and p(after, "skill3_ready_at") == ready and p(after, "skill3_active_until") == 0)
    if p(pc, "simulation_paused"):
        pc.action("Pause")
    pc.action("Speed3"); t0 = p(interior, "game_time"); pc.step_simulation(1.0); t1 = p(interior, "game_time")
    pc.action("Speed1"); pc.step_simulation(1.0); t2 = p(interior, "game_time")
    check("H-COOLDOWN-GAME-SPEED", abs((t1-t0)-3) < .06 and abs((t2-t1)-1) < .06 and p(interior.get_unit(uid), "skill3_ready_at") == ready,
          "game_delta_3x=%s game_delta_1x=%s" % (t1-t0, t2-t1))
    if not p(pc, "simulation_paused"):
        pc.action("Pause")
    check("H-DUTY-REASSIGN", interior.set_hero_duty(uid, True, bridge_id))
    interior.simulate(4.0)
    check("H-DUTY-RESTORED", p(interior.get_duty_bonuses(own), "speed_bonus") > 0)
    p(own, "systems").debug_destroy_slot(bridge_id, unreal.OVDamageCause.EXTERNAL)
    check("H-DESTROYED-FACILITY-NO-BONUS", p(interior.get_duty_bonuses(own), "speed_bonus") == 0)


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
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
    finish()
    unreal.unregister_slate_post_tick_callback(state["handle"])


state["handle"] = unreal.register_slate_post_tick_callback(tick)
