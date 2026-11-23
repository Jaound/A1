"""실제 PIE 렌더 프레임 사이에서 지도 클릭 영역과 혼성 함대전을 조회한다. OS 입력은 별도다."""
import json
import os
import statistics
import time
import traceback
from pathlib import Path

import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
saved = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))
output = saved / "Validation" / ("D126Integration_" + os.environ.get("OV_FLEET_RUN", "local") + ".json")
if output.exists():
    raise FileExistsError(output)
output.parent.mkdir(parents=True, exist_ok=True)
report = {"checks": [], "errors": [], "physical_input": False}
started = time.monotonic()


def check(name, ok, detail=""):
    report["checks"].append({"id": name, "passed": bool(ok), "detail": str(detail)})
    if not ok:
        raise AssertionError(f"{name}: {detail}")


def frames(count):
    for _ in range(count):
        yield


def verify():
    while not editor.get_game_world():
        yield
    pc = unreal.GameplayStatics.get_player_controller(editor.get_game_world(), 0)
    while not pc or not pc.get_hud():
        yield
        pc = unreal.GameplayStatics.get_player_controller(editor.get_game_world(), 0)
    check("함대전 시작", pc.start_play_scenario("Fleet100"))
    hud = pc.get_hud()
    hud.execute_ui_action("StarMapToggle")
    for scale in (.8, 1., 1.5):
        hud.set_user_ui_scale_multiplier(scale, False)
        yield from frames(6)
        for action in ("StarMapClose", "StarMapToggle", "StarSystem_Select_home_vanguard", "Env_BlackHole"):
            point = hud.get_action_center(action)
            hit = hud.get_hit_action_at_canvas_point(point)
            check(f"지도 {scale} {action}", str(hit) == action, (str(hit), point))
        background = hud.get_hit_action_at_canvas_point(unreal.Vector2D(4, 62))
        check(f"지도 {scale} 전장 차단", str(background) == "StarMapBackground", background)
    hud.execute_ui_action("StarMapClose")
    hud.set_user_ui_scale_multiplier(1, False)
    yield from frames(3)
    check("지도 닫힘", not hud.get_editor_property("star_map_open"))
    dev = pc.get_editor_property("development")
    hulls = ("corvette", "frigate", "repair", "mining", "assault", "transport", "vector", "bastion",
             "cruiser", "radar", "air_defense", "carrier", "battleship", "missile_ship", "dreadnought", "battlecarrier")
    for hull in hulls:
        pc.set_editor_property("developer_mode", True)
        ships = dev.spawn_ships(hull, 0, 1, unreal.Vector(-140000, 100000, 2000), 0, 0, 0, False, False)
        check(f"{hull} 생성", len(ships) == 1, dev.get_editor_property("last_message"))
        ship = ships[0]
        pc.set_editor_property("developer_mode", False)
        pc.select_ship(ship)
        hud.set_details_open(True)
        yield from frames(3)
        check(f"{hull} 프리뷰", hud.get_preview_mesh_count() > 0, hud.get_preview_mesh_count())
        hud.execute_ui_action("InteriorToggle")
        yield from frames(2)
        check(f"{hull} 선내 지도", hud.get_editor_property("interior_ship") == ship and hud.get_editor_property("interior_open"))
        hud.execute_ui_action("InteriorToggle")
        hud.set_details_open(False)
        pc.set_editor_property("developer_mode", True)
        check(f"{hull} 제거", dev.delete_ship(ship), dev.get_editor_property("last_message"))
    pc.set_editor_property("developer_mode", False)
    check("혼성 함대 재시작", pc.start_play_scenario("Fleet100"))
    pc.action("Pause")
    yield from frames(20)
    durations = []
    last = time.monotonic()
    while sum(durations) < 10.:
        yield
        now = time.monotonic()
        durations.append(now - last)
        last = now
    durations.sort()
    report["render"] = {"viewport": str(pc.get_viewport_size()), "frames": len(durations),
                        "elapsed_seconds": sum(durations), "mean_ms": statistics.mean(durations)*1000,
                        "p95_ms": durations[int(.95*(len(durations)-1))]*1000, "max_ms": durations[-1]*1000}
    combat = pc.get_editor_property("combat")
    check("렌더 중 실전 시간 진행", pc.get_play_scenario().get_editor_property("elapsed_seconds") > 0)
    report["render"]["shots"] = combat.get_editor_property("shot_count")
    report["render"]["hits"] = combat.get_editor_property("hit_count")
    report["render"]["ships"] = len(pc.get_editor_property("ships"))


def tick(_dt):
    try:
        if time.monotonic() - started > 150:
            raise TimeoutError("PIE 검사 시간 초과")
        next(steps)
        return
    except StopIteration:
        pass
    except Exception:
        report["errors"].append(traceback.format_exc())
    report["passed"] = not report["errors"]
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


steps = verify()
level.editor_request_begin_play()
handle = unreal.register_slate_post_tick_callback(tick)
