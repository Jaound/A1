"""Capture the 16 fleet catalog portraits and the selected hull from the actual launch menu."""
import json
from pathlib import Path
import time
import traceback
import unreal as u

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation" / "HullCardQuality"
out.mkdir(parents=True, exist_ok=True)
report = {"cards": {}}
started = time.monotonic()


def frames(count):
    for _ in range(count):
        yield


def wait_seconds(seconds):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        yield


def previews(hud, card_only):
    return {tex.get_path_name(): tex for tex in u.ObjectIterator(u.TextureRenderTarget2D)
            if tex.get_outer().get_class().get_name() == "OVShipPreview" and tex.get_outer().get_outer() == hud
            and (not card_only or tex.size_x == 384)}


def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = u.GameplayStatics.get_player_controller(world, 0)
    while not pc or not pc.get_hud():
        yield
        pc = u.GameplayStatics.get_player_controller(world, 0)
    hud = pc.get_hud()
    assert hud.begin_fleet_preparation("Fleet300", False)
    title = hud.get_title_screen()
    title.sortie_click("Next")
    catalog = [str(x) for x in pc.get_play_scenario().sortie_catalog()]
    rows = []
    for button in u.ObjectIterator(u.OVScenarioPickButton):
        row = button.get_content()
        if not isinstance(row,u.HorizontalBox) or row.get_children_count() != 2:
            continue
        label = row.get_child_at(1)
        if not isinstance(label,u.TextBlock) or not str(label.get_text()).startswith("+ "):
            continue
        target = row.get_child_at(0).get_content().get_content()
        target = target.brush.get_editor_property("resource_object")
        if isinstance(target,u.TextureRenderTarget2D) and target.get_outer().get_outer() == hud:
            rows.append((button.get_parent().get_child_index(button), target))
    assert len(rows) == len(catalog), (len(rows),len(catalog))
    cards = dict(zip(catalog,[target for _,target in sorted(rows)]))
    assert len(cards) == 16
    yield from wait_seconds(9)
    for hull, texture in cards.items():
        u.RenderingLibrary.export_render_target(world, texture, str(out), hull + ".exr")
        file = out / (hull + ".exr")
        assert file.stat().st_size > 1024, hull
        report["cards"][hull] = [texture.size_x, texture.size_y, file.stat().st_size]
    title.sortie_click("Add",catalog.index("repair"))
    yield from wait_seconds(2)
    large = [t for t in previews(title, False).values() if t.size_x > 384]
    assert large
    detail = max(large, key=lambda t: t.size_x)
    u.RenderingLibrary.export_render_target(world, detail, str(out), "repair-detail.exr")
    report["detail"] = [detail.size_x, detail.size_y]
    report["passed"] = True


def tick(_):
    try:
        if time.monotonic() - started > 180:
            raise TimeoutError("Hull preview capture timed out")
        next(job)
        return
    except StopIteration:
        pass
    except Exception:
        report["error"] = traceback.format_exc()
        report["passed"] = False
    (out / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()


job = run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle = u.register_slate_post_tick_callback(tick)
