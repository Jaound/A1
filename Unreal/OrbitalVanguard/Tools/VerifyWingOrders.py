"""진형과 항모 명령의 PIE HUD 검사. 마우스 입력과 별개로 기록한다."""
import json
import time
import traceback
from pathlib import Path

import unreal as u

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation" / time.strftime("WingHUD_%Y%m%d_%H%M%S")
out.mkdir(parents=True)
report = {"checks": [], "physical_input": False}
deadline = time.monotonic() + 120

def check(name, condition):
    report["checks"].append({"id": name, "passed": bool(condition)})
    if not condition:
        raise AssertionError(name)

def frames(count=8):
    for _ in range(count):
        yield

def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = u.GameplayStatics.get_player_controller(world, 0)
    hud = pc.get_hud()
    check("200코스트 장면", pc.start_play_scenario("Fleet200"))
    own = [s for s in pc.get_editor_property("ships") if s.get_editor_property("friendly")]
    carrier = next(s for s in own if s.get_editor_property("ship_name").startswith("항공모함"))
    pc.select_ship(carrier)
    for ship in own:
        if ship != carrier:
            pc.select_additional(ship)
    yield from frames()
    check("선택 카드의 진형 단추", hud.get_action_center("FormationMenu").x >= 0)
    hud.execute_ui_action("FormationMenu")
    yield from frames()
    for key in ("FormationScreen", "FormationCarrier", "FormationConvoy"):
        check(key, hud.get_action_center(key).x >= 0)
    hud.execute_ui_action("FormationScreen")
    yield from frames()
    check("전열 미리보기 적용 단추", hud.get_action_center("FormationApply").x >= 0)
    yield from capture(world, pc, "formation")
    hud.execute_ui_action("FormationApply")
    check("진형 이동 명령", "진형 이동" in pc.get_editor_property("last_action"))
    pc.select_ship(carrier)
    yield from frames()
    for key in ("AutoWing", "WingSupply", "LaunchAircraft", "RecallAircraft"):
        check(key, hud.get_action_center(key).x >= 0)
    check("도약과 항공 보급 단추 분리", hud.get_action_center("WingSupply").y - hud.get_action_center("PlotJump").y > 24)
    yield from capture(world, pc, "carrier")
    hud.execute_ui_action("LaunchAircraft")
    air = pc.get_editor_property("carrier")
    check("실제 80대 출격", air.count_aircraft(carrier, u.OVAircraftState.SORTIE) == 80)

def capture(world, pc, stem):
    path = out / (stem + ".png")
    u.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="' + path.as_posix() + '" -nosuffix', pc)
    end = time.monotonic() + 10
    while not path.exists() and time.monotonic() < end:
        yield
    check(stem + " 렌더", path.exists())

def tick(_delta):
    try:
        if time.monotonic() > deadline:
            raise TimeoutError("PIE HUD 시간 초과")
        next(job)
        return
    except StopIteration:
        report["passed"] = True
    except Exception:
        report.update(passed=False, error=traceback.format_exc())
    (out / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf8")
    u.unregister_slate_post_tick_callback(handle)
    u.OVDevelopmentEditorLibrary.restore_preview_settings()
    u.SystemLibrary.quit_editor()

job = run()
assert u.OVDevelopmentEditorLibrary.start_preview_window(1280, 740)
handle = u.register_slate_post_tick_callback(tick)
