"""실제 적함 진입 후 영웅 이동·교전의 정지 렌더 증거.

정상 임무 자동 명령과 PC.step_simulation(.05)만으로 진행한다. API 시간 진행이며
실제 마우스/키보드 입력, 실시간 조작감, 연속 애니메이션 검증을 뜻하지 않는다.
DEV 쉴드 정지·진입·승리·개별 인원 추가를 사용하지 않는다.
RunValidation.py의 OV_VALIDATOR로 실행하며 새 타임스탬프 폴더만 작성한다.
"""
import datetime
import json
import os
import time
import traceback
import unreal

stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
out = os.path.join(saved, "Validation", "InteriorPresentation", stamp)
os.makedirs(out, exist_ok=False)
GAME_LIMIT, WALL_LIMIT, STEP = 600.0, 180.0, .05
report = {"passed": None, "kind": "실제 진입 영웅 이동·교전 정지 렌더",
          "physical_input": False, "time_progression": "정상 자동 명령 + API 0.05초 전진",
          "scope": "정지 화면과 상태·이동·사격 기록. 연속 애니메이션·직접 조작감 검증 아님",
          "seed": 260112, "requested_size": [1920, 1080], "dev_shortcuts": [],
          "game_limit": GAME_LIMIT, "wall_limit": WALL_LIMIT,
          "captures": [], "checks": [], "errors": [],
          "observed_state_samples": {"Moving": 0, "Fighting": 0}}
state = {"stage": "start", "started": time.monotonic(), "at": time.monotonic(),
         "captured": set(), "finished": False}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def p(obj, key):
    return obj.get_editor_property(key)


def check(identifier, passed, detail=""):
    report["checks"].append({"id": identifier, "passed": bool(passed), "detail": str(detail)})
    if not passed:
        raise AssertionError(identifier + ": " + str(detail))


def pause(pc, enabled):
    if bool(p(pc, "simulation_paused")) != enabled:
        pc.action("Pause")


def vector(value):
    return [float(value.x), float(value.y), float(value.z)]


def unit_state(unit, interior):
    ship = p(unit, "ship")
    return {"unit_id": p(unit, "unit_id"), "person_id": p(unit, "person_id"),
            "name": p(unit, "name"), "role": str(p(unit, "role")), "state": str(p(unit, "state")),
            "hero_id": [p(p(unit, "hero_id"), key) for key in ("a", "b", "c", "d")],
            "ship": ship.get_path_name() if ship else None, "node_id": str(p(unit, "node_id")),
            "local_position": vector(p(unit, "local_position")), "path": [str(x) for x in p(unit, "path")],
            "hp": p(unit, "hp"), "max_hp": p(unit, "max_hp"), "progress": p(unit, "progress"),
            "distance_travelled": p(unit, "distance_travelled"), "shots": p(unit, "shots"),
            "hits": p(unit, "hits"), "skill_uses": p(unit, "skill_uses"),
            "enemy_id": p(unit, "enemy_id"), "interior_time": p(interior, "game_time")}


def score_state(mission):
    score = mission.get_score()
    return {key: p(score, key) for key in
            ("game_time", "shield_break_at", "first_entry_at", "shield_operation_at",
             "bridge_capture_at", "launched", "entered", "returned", "returned_after_entry")}


def finish():
    if state["finished"]:
        return
    state["finished"] = True
    pc = state.get("pc")
    if pc:
        pause(pc, True)
        mission = p(pc, "mission")
        report["final_score"] = score_state(mission)
        report["mission_result"] = str(p(mission, "result"))
    try:
        report["preview_settings_restored"] = bool(unreal.OVDevelopmentEditorLibrary.restore_preview_settings())
    except Exception:
        report["errors"].append(traceback.format_exc())
    report["passed"] = not report["errors"] and state["captured"] == {"Moving", "Fighting"} and all(
        item.get("exists", False) and item.get("state_stable", False) for item in report["captures"])
    report["elapsed_wall_seconds"] = time.monotonic() - state["started"]
    report["output_directory"] = out
    with open(os.path.join(out, "report.json"), "w", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state["handle"])


def eligible_units(pc):
    mission, interior, transport = p(pc, "mission"), p(pc, "interior"), p(pc, "transport")
    target = p(mission, "objective")
    if not target or not interior.is_structure_known(0, target):
        return []
    # 같은 Step의 Transport/Interior 진입이 다음 Mission 집계보다 먼저 보일 수 있다.
    # 정상 진입 점수가 갱신될 때까지 후보만 기다리고, 캡처 시 재검사는 유지한다.
    score = mission.get_score()
    if p(score, "shield_break_at") < 0 or p(score, "first_entry_at") < 0 or p(score, "entered") <= 0:
        return []
    result = []
    for unit in p(interior, "units"):
        if p(unit, "faction_id") != 0 or p(unit, "role") == unreal.OVInteriorRole.TROOPER or p(unit, "ship") != target or p(unit, "hp") <= 0:
            continue
        person = transport.get_person(p(unit, "person_id"))
        if p(person, "runtime_unit_id") == p(unit, "unit_id") and p(person, "state") == unreal.OVPassengerState.INSIDE_TARGET:
            result.append(unit)
    return result


def prepare_capture(pc, unit, category, previous_distance):
    pause(pc, True)
    mission, interior, hud = p(pc, "mission"), p(pc, "interior"), pc.get_hud()
    target = p(mission, "objective")
    if p(pc, "developer_mode"):
        pc.action("Developer")
    hud.set_details_open(False)
    hud.set_hero_panel_open(False)
    pc.select_ship(target)
    hero_panel = hud.get_hero_panel()
    check(category + "-ACTUAL-HERO-SELECTION", hero_panel.select_mission_hero(p(unit, "unit_id")))
    hud.handle_interior_action("InteriorClose")
    hud.handle_interior_action("InteriorToggle")
    layout = p(p(target, "systems"), "layout")
    slot = next((row for row in p(layout, "slots") if p(row, "slot_id") == p(unit, "node_id")), None)
    check(category + "-ACTUAL-DECK", slot is not None)
    deck = p(slot, "deck_id")
    hud.handle_interior_action("InteriorDeck_" + str(deck))
    hud.handle_interior_action("InteriorClear")
    hud.handle_interior_action("InteriorUnit_" + str(p(unit, "unit_id")))
    if str(p(unit, "target_slot")) not in ("None", ""):
        hud.handle_interior_action("InteriorSlot_" + str(p(unit, "target_slot")))
    check(category + "-HUD-BINDING", p(hud, "interior_open") and p(hud, "interior_ship") == target
          and p(hud, "interior_deck") == deck and list(p(hud, "interior_selected_units")) == [p(unit, "unit_id")])
    score = score_state(mission)
    check(category + "-NORMAL-ENTRY", score["shield_break_at"] >= 0 and score["first_entry_at"] >= 0 and score["entered"] > 0)
    number, label = (1, "실제_선내_이동") if category == "Moving" else (2, "실제_선내_교전")
    state["pending"] = {"category": category, "path": os.path.join(out, "%02d_%s.png" % (number, label)),
                        "observation": unit_state(unit, interior), "previous_step_distance": previous_distance,
                        "step_distance": p(unit, "distance_travelled") - previous_distance,
                        "score": score, "deck": deck, "viewport_size": list(pc.get_viewport_size()),
                        "paused_before_render": bool(p(pc, "simulation_paused"))}
    state.update(stage="render", at=time.monotonic())


def tick(_dt):
    try:
        if time.monotonic() - state["started"] >= WALL_LIMIT:
            raise RuntimeError("180초 제한: 이동·교전 두 상태를 모두 캡처하지 못함")
        if state["stage"] == "start":
            check("SEPARATE-1920-WINDOW", unreal.OVDevelopmentEditorLibrary.start_preview_window(1920, 1080))
            state["stage"] = "world"
            return
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or len(p(pc, "ships")) < 3 or not pc.get_hud():
            return
        state["pc"] = pc
        mission, interior = p(pc, "mission"), p(pc, "interior")
        if state["stage"] == "world":
            check("NORMAL-MISSION", mission.start_mission(pc, unreal.OVMissionMode.ATTACK, report["seed"], 4, 2, False), p(mission, "status"))
            mission.set_auto_player(True)
            pc.action("Speed1")
            pause(pc, True)
            state["stage"] = "advance"
            return
        if state["stage"] == "advance":
            pause(pc, False)
            try:
                for _ in range(40):
                    before = state.get("previous_distances", {})
                    pc.step_simulation(STEP)
                    current_units = eligible_units(pc)
                    state["previous_distances"] = {p(unit, "unit_id"): p(unit, "distance_travelled") for unit in current_units}
                    for unit in current_units:
                        category = "Moving" if p(unit, "state") == unreal.OVInteriorState.MOVING else "Fighting" if p(unit, "state") == unreal.OVInteriorState.FIGHTING else None
                        if category:
                            report["observed_state_samples"][category] += 1
                        if not category or category in state["captured"]:
                            continue
                        previous_distance = before.get(p(unit, "unit_id"), p(unit, "distance_travelled"))
                        if category == "Moving" and p(unit, "distance_travelled") - previous_distance <= .001:
                            continue
                        if category == "Fighting" and (p(unit, "shots") <= 0 or p(unit, "enemy_id") <= 0 or not interior.can_observe(0, p(unit, "enemy_id"))):
                            continue
                        prepare_capture(pc, unit, category, previous_distance)
                        return
                    if p(mission, "game_time") >= GAME_LIMIT:
                        raise RuntimeError("600게임초 제한: 필요한 실제 선내 상태가 부족함")
                    if p(mission, "result") != unreal.OVMissionResult.ONGOING:
                        raise RuntimeError("임무 종료 전 두 상태를 확보하지 못함: " + str(p(mission, "result")))
            finally:
                pause(pc, True)
            return
        if time.monotonic() - state["at"] < .75:
            return
        pending = state["pending"]
        current = unit_state(interior.get_unit(pending["observation"]["unit_id"]), interior)
        if state["stage"] == "render":
            check(pending["category"] + "-FROZEN-BEFORE-SHOT", current == pending["observation"])
            unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="%s" -nosuffix' % pending["path"].replace("\\", "/"), pc)
            state.update(stage="verify", at=time.monotonic())
            return
        if state["stage"] == "verify":
            pending["exists"] = os.path.isfile(pending["path"]) and os.path.getsize(pending["path"]) > 0
            pending["state_stable"] = current == pending["observation"] and bool(p(pc, "simulation_paused"))
            report["captures"].append(pending)
            check(pending["category"] + "-ACTUAL-RENDER", pending["exists"])
            check(pending["category"] + "-FROZEN-AFTER-SHOT", pending["state_stable"])
            state["captured"].add(pending["category"])
            if state["captured"] == {"Moving", "Fighting"}:
                finish()
            else:
                state["stage"] = "advance"
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
