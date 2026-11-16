"""실제 통합 시스템을 정상 명령으로 진행하는 PIE API 검사. OS 입력/렌더 성능 검사가 아니다."""
import collections
import json
import os
import time
import traceback
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
os.makedirs(os.path.join(ROOT, "Validation"), exist_ok=True)
REPORT = {"종류": "PIE API 실제 시스템 통합 / 직접 마우스·키보드 아님", "기획": "035a4fba81d26f1d67daa1f44bded0f7e8d3a434",
          "확정밸런스": False, "완료": False, "검사": [], "사례": [], "오류": []}
S = {"phase": 0, "at": time.monotonic(), "case": 0}
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EDITOR = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def p(o, k):
    return o.get_editor_property(k)


def check(name, value, detail=""):
    REPORT["검사"].append({"ID": name, "성공": bool(value), "상세": str(detail)})
    unreal.log("OVMISSION " + name + " = " + str(bool(value)))
    if not value:
        raise AssertionError(name + ": " + str(detail))


def save():
    REPORT["성공"] = bool(REPORT["검사"]) and all(c["성공"] for c in REPORT["검사"]) and not REPORT["오류"]
    with open(os.path.join(ROOT, "Validation", "IntegratedMission.json"), "w", encoding="utf-8") as f:
        json.dump(REPORT, f, ensure_ascii=False, indent=2)


def pause(pc, value):
    if p(pc, "simulation_paused") != value:
        pc.action("Pause")


def fields(obj, names):
    return {name: p(obj, name) for name in names}


def score(mission):
    s = mission.get_score()
    result = fields(s, ["game_time", "shield_break_at", "first_entry_at", "shield_operation_at", "bridge_capture_at",
                       "retreat_at", "last_return_at", "finished_at", "launched", "entered", "returned", "returned_after_entry",
                       "alive_when_retreat_ordered", "shield_reactivations"])
    result["factions"] = [fields(f, ["faction_id", "initial_cost", "living_cost", "ships_lost", "heroes", "heroes_dead",
                                         "heroes_rescue_waiting", "heroes_wounded", "troops", "troops_dead",
                                         "aircraft_lost", "aircraft_surviving"]) for f in p(s, "factions")]
    result["result"] = str(p(mission, "result"))
    result["phase"] = str(p(mission, "phase"))
    return result


def case_list():
    # 같은 편성: 함대31+31, 일반 수비/강습병 수, 후보 수치와 시드를 고정한다.
    cases = []
    for seed in [260112, 260113, 260114, 260115]:
        for mode, heroes, guards in [("FLEET_ONLY", 0, 0), ("ATTACK", 2, 0), ("ATTACK", 2, 1),
                                     ("ATTACK", 2, 2), ("ATTACK", 2, 4), ("DEFENSE", 2, 2)]:
            cases.append(dict(mode=mode, attack_heroes=heroes, defense_heroes=guards, seed=seed, speed=1, shuttle=False, limit=600))
    # 같은 실제 게임시간의 상태 비교. ID 값 자체는 장면 재시작마다 재사용하지 않는다.
    for speed in [1, 2, 3]:
        cases.append(dict(mode="ATTACK", attack_heroes=4, defense_heroes=0, seed=260112, speed=speed, shuttle=False, limit=120, clock=True))
    for shuttle in [False, True]:
        cases.append(dict(mode="ATTACK", attack_heroes=4, defense_heroes=0, seed=260112, speed=1, shuttle=shuttle, limit=600))
    if os.environ.get("OV_MISSION_CASES") == "smoke":
        return cases[-2:]
    return cases


def begin_case(pc, cfg):
    m = p(pc, "mission")
    check("MISSION-START-" + str(S["case"]), m.start_mission(pc, getattr(unreal.OVMissionMode, cfg["mode"]),
          cfg["seed"], cfg["attack_heroes"], cfg["defense_heroes"], cfg["shuttle"]))
    interior, transport = p(pc, "interior"), p(pc, "transport")
    units = p(interior, "units")
    check("MISSION-REAL-PERSONS-" + str(S["case"]), all(p(x, "runtime_unit_id") > 0 for x in p(transport, "persons")))
    check("MISSION-REAL-SHIELD-" + str(S["case"]), p(p(p(m, "objective"), "systems"), "shield_active"))
    check("MISSION-UNIQUE-IDS-" + str(S["case"]), len({p(x, "unit_id") for x in units}) == len(units)
          and len({p(x, "person_id") for x in units}) == len(units))
    check("MISSION-FACTION-LIMIT-" + str(S["case"]), interior.count_active_heroes(0) <= 15 and interior.count_active_heroes(1) <= 15)
    pause(pc, True)
    t = p(m, "game_time")
    pc.step_simulation(.5)
    check("MISSION-PAUSE-" + str(S["case"]), p(m, "game_time") == t)
    m.set_auto_player(True)
    pc.action("Speed" + str(cfg["speed"]))
    S.update(mission=m, cfg=cfg, elapsed=0., wall=time.monotonic(), initial_ids=[p(x, "unit_id") for x in units])


def finish_case(pc):
    m, cfg = S["mission"], S["cfg"]
    data = score(m)
    data["조건"] = cfg
    data["시간제한도달"] = S["elapsed"] >= cfg["limit"] and p(m, "result") == unreal.OVMissionResult.ONGOING
    data["API벽시계초"] = round(time.monotonic()-S["wall"], 3)
    units = p(p(pc, "interior"), "units")
    data["units"] = [dict(fields(u, ["unit_id", "person_id", "faction_id", "hp", "distance_travelled", "shots", "hits", "death_events"]),
                          state=str(p(u, "state")), role=str(p(u, "role"))) for u in units]
    data["persons"] = [dict(person_id=p(x, "person_id"), state=str(p(x, "state")), pod_id=p(x, "pod_id"))
                       for x in p(p(pc, "transport"), "persons")]
    check("MISSION-NO-REGEN-" + str(S["case"]), [p(x, "unit_id") for x in units] == S["initial_ids"])
    check("MISSION-ONCE-DEATH-" + str(S["case"]), all(p(x, "death_events") <= 1 for x in units))
    check("MISSION-RETURN-NOT-OVERCOUNT-" + str(S["case"]), data["returned"] <= data["launched"]
          and data["returned_after_entry"] <= data["entered"] <= data["launched"])
    if data["first_entry_at"] >= 0:
        check("MISSION-ACTUAL-SHIELD-FIRST-" + str(S["case"]), 0 <= data["shield_break_at"] <= data["first_entry_at"])
    if data["shield_operation_at"] >= 0:
        check("MISSION-ACTUAL-TRAVEL-" + str(S["case"]), any(p(x, "distance_travelled") > 100 for x in units))
    data["game_time"] = round(data["game_time"], 4)
    REPORT["사례"].append(data)
    save()
    S["case"] += 1


def finish(pc):
    clocks = [c for c in REPORT["사례"] if c["조건"].get("clock")]
    def comparable(c):
        # 실수 시계 오차와 실행/인물 ID를 제외하고 실제 손실·HP·행동을 비교한다.
        return {"score": {k: c[k] for k in ["factions", "launched", "entered", "returned", "returned_after_entry", "result", "phase"]},
                "units": [{k: (round(v, 3) if isinstance(v, float) else v) for k, v in u.items() if k not in ["unit_id", "person_id"]}
                          for u in c["units"]]}
    if clocks:
        check("MISSION-CLOCK-123", len(clocks) == 3 and comparable(clocks[0]) == comparable(clocks[1]) == comparable(clocks[2]))
    completed = [c for c in REPORT["사례"] if c["bridge_capture_at"] >= 0 and c["returned_after_entry"] > 0]
    check("MISSION-END-TO-END-REAL-CAPTURE-RETURN", bool(completed), "실제 쉴드·운송·점령·철수·회수까지 완주한 사례 수=" + str(len(completed)))
    check("MISSION-REAL-SHIELD-OPERATION", any(c["shield_operation_at"] >= 0 for c in REPORT["사례"]))
    pc.start_combat_range()
    check("MISSION-F5-RESET", not p(p(pc, "mission"), "enabled") and not p(p(pc, "interior"), "enabled"))
    pc.start_calibration_range(1)
    pc.start_phase2c_range(5)
    check("MISSION-F7-LEGACY-DEV", not p(p(pc, "interior"), "enabled") and any(p(x, "runtime_unit_id") == 0 for x in p(p(pc, "transport"), "persons")))
    p(pc, "mission").start_mission(pc)
    pause(pc, True)
    REPORT["완료"] = True
    save()
    unreal.unregister_slate_post_tick_callback(S["handle"])
    unreal.log("OVMISSION COMPLETE")
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    try:
        if S["phase"] == 0:
            LEVEL.editor_request_begin_play()
            S.update(phase=1, at=time.monotonic())
            return
        world = EDITOR.get_game_world()
        if not world:
            if time.monotonic()-S["at"] > 60:
                raise RuntimeError("PIE 월드 준비 시간 초과")
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not isinstance(pc, unreal.OVController) or not p(pc, "mission"):
            return
        if S["phase"] == 1:
            S["cases"] = case_list()
            begin_case(pc, S["cases"][0]); S["phase"] = 2
            return
        if S["phase"] == 2:
            cfg = S["cfg"]
            # 비렌더 API 가속임을 명시한다. 프레임별 실제 공유 시계만 호출한다.
            pause(pc, False)
            for _ in range(40):
                if S["elapsed"] >= cfg["limit"] or p(S["mission"], "result") != unreal.OVMissionResult.ONGOING:
                    break
                pc.step_simulation(.25/cfg["speed"])
                S["elapsed"] += .25
            pause(pc, True)
            if S["elapsed"] >= cfg["limit"] or p(S["mission"], "result") != unreal.OVMissionResult.ONGOING:
                finish_case(pc)
                if S["case"] >= len(S["cases"]):
                    finish(pc)
                else:
                    begin_case(pc, S["cases"][S["case"]])
    except Exception:
        REPORT["오류"].append(traceback.format_exc()); save()
        unreal.log_error(REPORT["오류"][-1])
        unreal.unregister_slate_post_tick_callback(S["handle"])
        unreal.SystemLibrary.quit_editor()


S["handle"] = unreal.register_slate_post_tick_callback(tick)
