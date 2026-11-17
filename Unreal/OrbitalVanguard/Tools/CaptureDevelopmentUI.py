"""Capture the PIE UI using reflected widget functions, not OS mouse/keyboard input.

Run through Tools/RunValidation.py with OV_VALIDATOR pointing here. Set
OV_UI_CAPTURE_RES=1920x1080 or 960x600 to request a viewport resolution; omit it
to preserve the current PIE viewport. Embedded PIE may ignore r.SetRes: the
report records actual viewport dimensions and does not claim requested sizing.
Each invocation writes a timestamped directory under Saved/Validation so an old
screenshot cannot make this run pass. This is visual evidence, not a click,
keyboard, focus, gameplay, layout, or camera gesture test.
"""
import json
import os
import time
import traceback
import unreal

saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
run_id = time.strftime("%Y%m%d_%H%M%S") + "_%06d" % (time.time_ns() % 1000000)
out_dir = os.path.join(saved, "Validation", "DevelopmentUI", run_id)
os.makedirs(out_dir, exist_ok=False)
requested = os.environ.get("OV_UI_CAPTURE_RES", "").strip().lower()
report = {"kind": "PIE UI render capture via reflected widget functions", "physical_input": False,
          "requested_resolution": requested or None, "screenshots": [], "errors": [],
          "scope": "Reflection calls are not physical input, focus, or gameplay validation."}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state = {"stage": "start", "started": time.monotonic(), "at": time.monotonic(), "index": 0}
views = [("01_play_hero", None), ("02_dev_overview", "통합 현황"),
         ("03_dev_hulls", "함선·초소형함"), ("04_dev_heroes", "영웅"),
         ("05_dev_weapons", "무장")]


def finish():
    report["passed"] = not report["errors"] and len(report["screenshots"]) == len(views) and all(
        entry["exists"] for entry in report["screenshots"])
    report["elapsed_wall_seconds"] = time.monotonic() - state["started"]
    with open(os.path.join(out_dir, "report.json"), "w", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    unreal.log("UI capture report: " + os.path.join(out_dir, "report.json"))
    unreal.unregister_slate_post_tick_callback(state["handle"])


def select_tab(widget, label):
    # Direct reflected UFUNCTION call. This does not synthesize a physical click.
    functions = {"통합 현황": "Overview", "함선·초소형함": "Hulls",
                 "영웅": "Heroes", "무장": "Weapons"}
    widget.call_method(functions[label])


def viewport_size(pc):
    width, height = pc.get_viewport_size()
    return [int(width), int(height)]


def tick(_dt):
    try:
        elapsed = time.monotonic() - state["started"]
        if elapsed > 90:
            raise RuntimeError("캡처 시간 초과")
        if state["stage"] == "start":
            if requested and requested not in ("1920x1080", "960x600"):
                raise RuntimeError("지원 요청 해상도는 1920x1080 또는 960x600")
            level.editor_request_begin_play()
            state["stage"] = "wait_pie"
            return
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or len(pc.get_editor_property("ships")) < 3:
            if elapsed > 30:
                raise RuntimeError("30초 내 PIE 플레이어를 찾지 못함")
            return
        if state["stage"] == "wait_pie":
            if requested:
                unreal.SystemLibrary.execute_console_command(world, "r.SetRes " + requested + "w", pc)
            if pc.get_editor_property("developer_mode"):
                pc.action("Developer")
            state.update(stage="prepare", at=time.monotonic())
            return
        if time.monotonic() - state["at"] < 1.0:
            return
        if state["stage"] == "prepare":
            name, tab = views[state["index"]]
            if tab is not None:
                if not pc.get_editor_property("developer_mode"):
                    pc.action("Developer")
                hud = pc.get_hud()
                if not hud:
                    raise RuntimeError("PIE HUD를 찾지 못함")
                try:
                    widget = hud.get_editor_property("development_widget")
                except Exception:
                    widget = hud.get_editor_property("DevelopmentWidget")
                if not widget:
                    raise RuntimeError("HUD의 개발 위젯이 생성되지 않음")
                select_tab(widget, tab)
            state.update(stage="capture", at=time.monotonic())
            return
        if state["stage"] == "capture":
            name, tab = views[state["index"]]
            path = os.path.abspath(os.path.join(out_dir, name + ".png")).replace("\\", "/")
            actual = viewport_size(pc)
            requested_pixels = [int(part) for part in requested.split("x")] if requested else None
            state["pending"] = {"view": name, "tab": tab, "path": path,
                                "viewport_pixels": actual,
                                "requested_resolution_matched": actual == requested_pixels if requested_pixels else None}
            unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="' + path + '" -nosuffix', pc)
            state.update(stage="verify", at=time.monotonic())
            return
        if state["stage"] == "verify":
            item = state["pending"]
            item["exists"] = os.path.isfile(item["path"]) and os.path.getsize(item["path"]) > 0
            report["screenshots"].append(item)
            if not item["exists"]:
                raise RuntimeError("스크린샷 파일이 생성되지 않음: " + item["path"])
            state["index"] += 1
            if state["index"] == len(views):
                finish()
                return
            state.update(stage="prepare", at=time.monotonic())
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
