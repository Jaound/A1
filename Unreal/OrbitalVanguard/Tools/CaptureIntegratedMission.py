"""정상 통합 임무의 API 진행과 UE 렌더 캡처. 실제 마우스·키보드 검사가 아니다.

RunValidation.py의 OV_VALIDATOR로 실행할 수 있다. DEV 쉴드 정지/진입/성공을
사용하지 않고 정상 자동 명령이 쉴드실에 도달하면 정지 화면 두 장을 저장한다.
실패·시간 초과도 실제 상태 그대로 남긴다. 캡처 성공은 임무 성공 판정이 아니다.
"""
import json
import os
import time
import traceback
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
out_dir = os.path.join(root, "Validation", "IntegratedMissionCapture")
os.makedirs(out_dir, exist_ok=True)
game_limit = float(os.environ.get("OV_CAPTURE_GAME_LIMIT", "600"))
wall_limit = float(os.environ.get("OV_CAPTURE_WALL_LIMIT", "240"))
report = {"kind": "정상 통합 임무 API 진행·UE 렌더 캡처", "physical_input": "수행하지 않음",
          "seed": 260112, "dev_shortcuts": [], "screenshots": [], "errors": [],
          "game_limit": game_limit, "wall_limit": wall_limit,
          "scope": "정지 캡처는 조작감·임무 완주·렌더 장기 성능 검증을 대신하지 않는다."}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state = {"phase": 0, "started": time.monotonic(), "at": time.monotonic()}


def p(obj, key):
    return obj.get_editor_property(key)


def pause(pc, value):
    if p(pc, "simulation_paused") != value:
        pc.action("Pause")


def score_dict(mission):
    score = mission.get_score()
    return {key: p(score, key) for key in
            ("game_time", "shield_break_at", "first_entry_at", "shield_operation_at",
             "bridge_capture_at", "retreat_at", "last_return_at", "finished_at",
             "launched", "entered", "returned", "returned_after_entry", "shield_reactivations")}


def finish():
    # passed는 파일 증거 생성만 뜻한다. 쉴드실 작전과 임무 결과는 별도 필드다.
    report["passed"] = not report["errors"] and len(report["screenshots"]) == 2 and all(
        item["exists"] for item in report["screenshots"])
    report["elapsed_wall_seconds"] = time.monotonic() - state["started"]
    with open(os.path.join(root, "Validation", "IntegratedMissionCapture.json"), "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state["handle"])


def capture(world, pc, name):
    path = os.path.abspath(os.path.join(out_dir, name + ".png")).replace("\\", "/")
    # -nosuffix keeps the evidence path stable; existing screenshots are retained by label.
    unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="' + path + '" -nosuffix', pc)
    state["path"] = path
    state["at"] = time.monotonic()


def tick(dt):
    try:
        elapsed = time.monotonic() - state["started"]
        if state["phase"] == 0:
            level.editor_request_begin_play(); state["phase"] = 1; return
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or len(p(pc, "ships")) < 3:
            if elapsed > 30:
                raise RuntimeError("30초 안에 PIE 컨트롤러를 찾지 못함")
            return
        mission, hud = p(pc, "mission"), pc.get_hud()
        if state["phase"] == 1:
            if not mission.start_mission(pc, unreal.OVMissionMode.ATTACK, report["seed"], 4, 2, False):
                raise RuntimeError("정상 통합 임무 시작 실패")
            pc.set_editor_property("developer_mode", False)
            hud.set_details_open(False); hud.handle_interior_action("InteriorClose")
            pc.action("Speed1"); pause(pc, True)
            mission.set_auto_player(True)
            state["phase"] = 2; return
        if state["phase"] == 2:
            pause(pc, False)
            for _ in range(10):
                pc.step_simulation(.2)
                score = mission.get_score()
                if p(score, "shield_operation_at") >= 0 or p(mission, "result") != unreal.OVMissionResult.ONGOING:
                    break
            pause(pc, True)
            score = mission.get_score()
            reached = p(score, "shield_operation_at") >= 0
            ended = p(mission, "result") != unreal.OVMissionResult.ONGOING
            timed_out = p(mission, "game_time") >= game_limit or elapsed > wall_limit
            if not (reached or ended or timed_out):
                return
            report.update({"shield_operation_achieved": reached, "mission_result": str(p(mission, "result")),
                           "mission_phase": str(p(mission, "phase")), "status": mission.get_public_status(),
                           "stop_reason": "쉴드실 실제 작전 도달" if reached else "임무 종료" if ended else "시간 제한",
                           "score": score_dict(mission), "structure_observed": p(pc, "interior").is_structure_known(0, p(mission, "objective"))})
            pc.frame_engagement()
            state["phase"] = 3; state["at"] = time.monotonic(); return
        if time.monotonic() - state["at"] < .75:
            return
        if state["phase"] == 3:
            capture(world, pc, "01_기본_플레이"); state["phase"] = 4; return
        if state["phase"] == 4:
            report["screenshots"].append({"path": state["path"], "exists": os.path.isfile(state["path"]), "view": "기본 플레이"})
            objective = p(mission, "objective")
            pc.select_ship(objective)
            hud.handle_interior_action("InteriorMissionToggle")
            hud.handle_interior_action("InteriorAll")
            shield_slot = str(p(mission, "shield_slot"))
            if objective:
                slots = p(p(p(objective, "systems"), "layout"), "slots")
                selected = next((slot for slot in slots if str(p(slot, "slot_id")) == shield_slot), None)
                if selected:
                    hud.handle_interior_action("InteriorDeck_" + str(p(selected, "deck_id")))
                hud.handle_interior_action("InteriorSlot_" + shield_slot)
            state["phase"] = 5; state["at"] = time.monotonic(); return
        if state["phase"] == 5:
            capture(world, pc, "02_실제_선내지도"); state["phase"] = 6; return
        if state["phase"] == 6:
            report["screenshots"].append({"path": state["path"], "exists": os.path.isfile(state["path"]), "view": "선내 지도 · 미관측이면 잠김 유지"})
            finish()
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
