"""실제 PIE 뷰포트에서 글꼴 검수용 화면을 저장한다. 입력 장치 검사는 별도다."""
import json
import struct
import time
import traceback
from pathlib import Path

import unreal as ue

output = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_saved_dir())) / "Validation/TypographyPanels"
output.mkdir(parents=True, exist_ok=True)
editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
result = {"screens": []}


def capture(world, controller, name):
    frame = output / (name + ".png")
    previous = frame.stat().st_mtime_ns if frame.exists() else 0
    for _ in range(8):
        yield
    # PIE 설명 창이나 다른 뷰포트의 스크린샷이 돌아오면 게임 창에서 다시 캡처한다.
    reference = output / "Pause.png"
    expected = reference.read_bytes()[16:24] if name != "Pause" and reference.exists() else None
    for _ in range(3):
        ue.SystemLibrary.execute_console_command(world, f'Shot SHOWUI filename="{frame.as_posix()}" -nosuffix', controller)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            yield
            if frame.exists() and frame.stat().st_mtime_ns > previous:
                try:
                    dims = frame.read_bytes()[16:24]
                except OSError:
                    continue
                if len(dims) != 8:
                    continue
                previous = frame.stat().st_mtime_ns
                if expected and expected != dims:
                    break
                result.setdefault("sizes", {})[name] = struct.unpack(">II", dims)
                result["screens"].append(name)
                return
    raise RuntimeError("화면 저장 시간 초과 " + name)


def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    controller = ue.GameplayStatics.get_player_controller(world, 0)
    while not controller or not controller.get_hud():
        yield
        controller = ue.GameplayStatics.get_player_controller(world, 0)
    assert controller.start_play_scenario("Fleet50")
    hud = controller.get_hud()
    hud.close_title_screen()
    hud.execute_ui_action("SystemMenu")
    yield from capture(world, controller, "Pause")
    hud.execute_ui_action("PauseResume")
    hud.execute_ui_action("StarMapToggle")
    yield from capture(world, controller, "Galaxy")
    hud.execute_ui_action("StarMapToggle")
    ship = controller.get_editor_property("ships")[0]
    controller.select_ship(ship)
    hud.set_details_open(True)
    yield from capture(world, controller, "Ship")
    hud.set_details_open(False)
    if controller.get_editor_property("simulation_paused"):
        controller.action("Pause")
    combat = controller.get_editor_property("combat")
    for step in range(4000):
        controller.step_simulation(.05)
        if step % 30 == 0:
            yield
            if combat.get_editor_property("recent_events"):
                break
    result["combatEvents"] = len(combat.get_editor_property("recent_events"))
    assert result["combatEvents"] > 0, "전투 발생 확인 실패"
    target = next((event.get_editor_property("target") for event in reversed(combat.get_editor_property("recent_events")) if event.get_editor_property("target")), ship)
    controller.select_ship(target)
    controller.set_editor_property("camera_focus", target.get_actor_location())
    controller.set_editor_property("camera_distance", 19000)
    controller.update_combat_presentation()
    yield from capture(world, controller, "Battle")

    assert controller.start_play_scenario("HeroArena")
    controller.action("ArenaAdd")
    controller.action("ArenaFill")
    interior = controller.get_editor_property("interior")
    iris = next(unit for unit in interior.get_editor_property("units") if str(unit.get_editor_property("definition_id")) == "M02")
    hud.set_hero_panel_open(True)
    for _ in range(12):
        yield
    panel = hud.get_hero_panel()
    assert panel.select_hero(iris.get_editor_property("hero_id"))
    for _ in range(12):
        yield
    detail = next(label for label in ue.ObjectIterator(ue.TextBlock)
                  if label.get_path_name().startswith(panel.get_path_name() + ".")
                  and "이리스 베인" in str(label.get_text()) and "\n" in str(label.get_text()))
    tooltip = detail.get_editor_property("tool_tip_widget")
    label = tooltip.get_content()
    result["tooltipChars"] = len(str(label.get_text()))
    result["tooltipWrap"] = label.get_editor_property("wrap_text_at")
    assert result["tooltipChars"] > 50 and result["tooltipWrap"] == 360
    # Slate의 별도 팝업 창은 뷰포트 캡처에 포함되지 않는다. 배포 위젯의 내용과 줄바꿈을 검수한다.
    viewport = ue.get_engine_subsystem(ue.GameViewportSubsystem)
    slot = ue.GameViewportWidgetSlot()
    slot.set_editor_property("anchors", ue.Anchors(ue.Vector2D(0,0), ue.Vector2D(0,0)))
    slot.set_editor_property("offsets", ue.Margin(45, 110, 0, 0))
    slot.set_editor_property("z_order", 199)
    assert viewport.add_widget(tooltip, slot)
    try:
        yield from capture(world, controller, "HeroTooltip")
    finally:
        viewport.remove_widget(tooltip)


def tick(_delta):
    try:
        next(job)
        return
    except StopIteration:
        result["passed"] = True
    except Exception:
        result["error"] = traceback.format_exc()
    (output / "Result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    ue.unregister_slate_post_tick_callback(handle)
    ue.SystemLibrary.quit_editor()


job = run()
ue.get_editor_subsystem(ue.LevelEditorSubsystem).editor_request_begin_play()
handle = ue.register_slate_post_tick_callback(tick)
