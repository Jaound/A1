"""별도 PIE 창에서 선택 카드의 화면, 페이지, 이름과 입력 영역을 확인한다."""
import json
import os
import struct
import time
import traceback
from pathlib import Path

import unreal as ue

width, height = map(int, os.getenv("OV_ROSTER_SIZE", "1920x1080").split("x"))
baseline = os.getenv("OV_ROSTER_BASELINE") == "1"
scale = float(os.getenv("OV_ROSTER_SCALE", "1"))
folder = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_saved_dir())) / "Validation" / "FleetRoster"
folder.mkdir(parents=True, exist_ok=True)
label = f"{width}x{height}_{scale:g}_" + ("before" if baseline else "after")
checks = []
editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
started = time.monotonic()


def verify(name, condition):
    if not condition:
        raise AssertionError(name)
    checks.append(name)


def settle():
    for _ in range(24):
        yield


def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = ue.GameplayStatics.get_player_controller(world, 0)
    while not pc or not pc.get_hud():
        yield
        pc = ue.GameplayStatics.get_player_controller(world, 0)
    hud = pc.get_hud()
    verify("50코스트전 시작", pc.start_play_scenario("Fleet50"))
    hud.set_user_ui_scale_multiplier(scale, False)
    yield from settle()
    ships = list(pc.get_editor_property("ships"))
    allies = {i for i, ship in enumerate(ships) if ship.get_editor_property("friendly")}
    count = pc.select_fleet_rectangle(ue.Vector2D(-10000,-10000), ue.Vector2D(10000,10000), False)
    verify("아군 전체 선택", count == len(allies) and count > 6)
    yield from settle()
    path = folder / (label+".png")
    path.unlink(missing_ok=True)
    ue.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="'+path.as_posix()+'" -nosuffix', pc)
    while not path.exists() or path.stat().st_size < 24:
        yield
    size = struct.unpack(">II",path.read_bytes()[16:24])
    verify("실제 화면 캡처", size[0]>=width and size[1]>=height)
    seen = set()
    pages = 0
    for _ in range(count):
        page = set()
        for i, ship in enumerate(ships):
            action = "Pick_" + str(i)
            pt = hud.get_action_center(action)
            if pt.x < 0:
                continue
            verify("카드 선택 영역 "+str(i), str(hud.get_hit_action_at_canvas_point(pt)) == action)
            name = str(hud.get_fleet_title_tooltip(pt))
            verify("실제 함선 이름 "+str(i), i in allies and name == ship.get_editor_property("ship_name"))
            page.add(i)
        verify("페이지별 고유 함선", bool(page) and not (seen & page))
        pages += 1
        seen.update(page)
        if seen == allies:
            break
        hud.execute_ui_action("FleetNext")
        yield from settle()
    verify("전체 아군 접근", seen == allies)
    if not baseline:
        verify("여러 페이지 전환", pages>1)
        hud.execute_ui_action("FleetPrevious")
        yield from settle()
        pc.select_ship(ships[min(allies)])
        if not hud.get_editor_property("details_open"):
            hud.execute_ui_action("Details")
        yield from settle()
        hud.execute_ui_action("WindowMax")
        yield from settle()
        captures = [tex for tex in ue.ObjectIterator(ue.TextureRenderTarget2D)
                    if tex.get_outer().get_class().get_name()=="OVShipPreview" and tex.get_outer().get_outer()==hud]
        verify("확대한 선체 미리보기의 실제 해상도", any(tex.get_editor_property("size_x") >= width*.85 for tex in captures))
        hud.execute_ui_action("Details")
    pc.clear_selection()
    yield from settle()
    verify("선택 해제 후 카드 숨김", all(hud.get_action_center("Pick_"+str(i)).x<0 for i in allies))


def tick(dt):
    error = None
    try:
        if time.monotonic()-started>150:
            raise TimeoutError("화면 검사 시간 초과")
        next(job)
        return
    except StopIteration:
        pass
    except Exception:
        error = traceback.format_exc()
    (folder/(label+".json")).write_text(json.dumps(dict(passed=not error, checks=checks, error=error, physical_input=False), ensure_ascii=False, indent=2),encoding="utf8")
    ue.unregister_slate_post_tick_callback(handle)
    ue.OVDevelopmentEditorLibrary.restore_preview_settings()
    ue.SystemLibrary.quit_editor()


job = run()
assert ue.OVDevelopmentEditorLibrary.start_preview_window(width, height)
handle = ue.register_slate_post_tick_callback(tick)
