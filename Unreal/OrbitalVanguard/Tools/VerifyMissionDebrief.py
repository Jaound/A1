"""작은 별도 PIE 창에서 임무 카드와 결과의 표시·재진입을 검사한다."""
import json
import struct
import time
import traceback
from pathlib import Path
import unreal as u

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
root = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))
out = root / "Validation" / time.strftime("MissionDebrief_%Y%m%d_%H%M%S")
out.mkdir(parents=True)
report = {"checks": [], "physical_input": False}
deadline = time.monotonic() + 150

def check(label, ok):
    if not ok:
        raise AssertionError(label)
    report["checks"].append(label)

def frames(n=8):
    for _ in range(n):
        yield

def photo(world, pc, name):
    path = out / (name + ".png")
    for attempt in range(3):
        yield from frames(45)
        candidate = out / f"{name}_{attempt}.png"
        u.SystemLibrary.execute_console_command(world, f'Shot SHOWUI filename="{candidate.as_posix()}" -nosuffix', pc)
        until = time.monotonic() + 6
        while not candidate.exists() and time.monotonic() < until:
            yield
        if candidate.exists():
            width, height = struct.unpack(">II", candidate.read_bytes()[16:24])
            if 960 <= width < 980 and 540 <= height < 600:
                candidate.replace(path)
                break
    check(name + " 플레이 창 렌더 저장", path.exists())

def verify():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = u.GameplayStatics.get_player_controller(world, 0)
    while not pc or not pc.get_hud():
        yield
        pc = u.GameplayStatics.get_player_controller(world, 0)
    hud = pc.get_hud()
    play = pc.get_play_scenario()
    hud.open_title_screen()
    title = hud.get_title_screen()
    title.show_panel(u.OVTitleMenuPanel.SCENARIO_SELECT)
    for scene in ["Fleet50","Fleet100", "Fleet200", "BlackHoleTrial", "Logistics", "Evacuation", "Sandbox", "Fleet300"]:
        title.select_scenario(scene)
        texture = u.load_asset("/Game/UI/MissionCards/" + scene)
        check(scene + " 카드 선택·이미지", isinstance(texture, u.Texture2D) and str(title.get_selected_scenario_id()) == scene)
    yield from photo(world, pc, "cards")
    check("실제 임무 진입", pc.start_play_scenario("Fleet100") and not hud.is_title_screen_open())
    for was_paused in (True, False):
        if not was_paused: pc.action("Pause")
        ship = pc.get_editor_property("ships")[0]
        hud.execute_ui_action("MissionCards")
        check(f"카드 열람 일시정지 {was_paused}", hud.is_title_screen_open() and pc.get_editor_property("simulation_paused"))
        hud.close_title_screen()
        check(f"원래 함대·시간 상태 복귀 {was_paused}", pc.get_editor_property("ships")[0] == ship and pc.get_editor_property("simulation_paused") == was_paused)
    pc.action("Pause")
    # Boundary fixture: leave one friendly ship and all hostile ships via the combat departure event.
    combat = pc.get_editor_property("combat")
    friends = []
    for ship in pc.get_editor_property("ships"):
        if ship.get_editor_property("systems").get_editor_property("faction_id") == 1:
            combat.notify_ftl_departure(ship)
        else:
            friends.append(ship)
    combat.notify_ftl_departure(friends[0])
    pc.action("Pause")
    for _ in range(8):
        pc.step_simulation(.25)
    lines = list(play.get_editor_property("debrief_lines"))
    check("이탈·완파 분리", "이탈 1척" in lines[0] and "완파 0척" in lines[0] and f"전장 {len(friends)-1}척" in lines[0])
    check("승리 판정", play.get_editor_property("result") == u.OVPlaySceneResult.VICTORY)
    yield from photo(world, pc, "result")
    check("결과 버튼만 활성", hud.get_action_center("ResultField").x >= 0 and hud.get_action_center("SceneMenu").x < 0)
    hud.execute_ui_action("ResultField")
    yield from frames()
    check("전장 재열람", hud.get_action_center("ShowResult").x >= 0)
    hud.execute_ui_action("ShowResult")
    yield from frames()
    check("재열람 중 집계 불변", lines == list(play.get_editor_property("debrief_lines")))
    hud.execute_ui_action("RestartScene")
    check("재도전 클릭이 새 전장에 전달되지 않음", hud.handle_pointer(500, 320, True, True, 0, False, 0, 0))
    check("버튼 해제까지 결과창이 소비", hud.handle_pointer(500, 320, False, False, 0, False, 0, 0))
    check("다음 전장 입력 허용", not hud.handle_pointer(500, 320, False, False, 0, False, 0, 0))
    check("재도전 집계 초기화", not play.get_editor_property("debrief_lines") and play.get_editor_property("result") == u.OVPlaySceneResult.RUNNING)
    hud.execute_ui_action("PlaySceneExit")
    check("목록 복귀·직전 카드 유지", hud.is_title_screen_open() and title.get_current_panel()==u.OVTitleMenuPanel.SCENARIO_SELECT and str(title.get_selected_scenario_id())=="Fleet100")
    check("종료 뒤 이전 임무 해제", str(play.get_editor_property("active_id"))=="None")

def tick(dt):
    try:
        if time.monotonic() > deadline:
            raise TimeoutError("PIE 검사 시간 초과")
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

job = verify()
assert u.OVDevelopmentEditorLibrary.start_preview_window(960, 540)
handle = u.register_slate_post_tick_callback(tick)
