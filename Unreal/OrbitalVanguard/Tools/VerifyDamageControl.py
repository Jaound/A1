"""실제 PIE의 임무 배치, 클릭 명령 처리, 내부 정찰 기록과 시설 경보를 검사한다."""
import os
import json
import time
import traceback
from pathlib import Path
import unreal as u

out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation" / "DamageControl"
out.mkdir(parents=True, exist_ok=True)
results = {"checks": [], "ranges_m": {}, "physical_input": False}
editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)


def prop(obj, key):
    return obj.get_editor_property(key)


def check(label, value):
    if not value:
        raise AssertionError(label)
    results["checks"].append(label)


def frames(n=8):
    for _ in range(n):
        yield


def run():
    for _ in range(1200):
        world = editor.get_game_world()
        pc = u.GameplayStatics.get_player_controller(world, 0) if world else None
        if pc and pc.get_hud():
            break
        yield
    hud = pc.get_hud()
    for mission in ("Fleet50", "Fleet100", "Fleet300", "BlackHoleTrial", "Fleet200"):
        check(mission + " 시작", pc.start_play_scenario(mission))
        ships = list(prop(pc, "ships"))
        allies = [s for s in ships if prop(s, "friendly")]
        enemies = [s for s in ships if not prop(s, "friendly")]
        combat = prop(pc, "combat")
        longest = max(prop(prop(m, "profile"), "range") for s in ships for m in prop(combat.get_ship_state(s), "mounts"))
        gap = min((a.get_actor_location() - b.get_actor_location()).length() for a in allies for b in enemies)
        results["ranges_m"][mission] = [round(gap / 100), round(longest / 100)]
        check(mission + " 전방 함선이 최장 사거리 밖에서 시작", gap > longest + 20000)
    if not prop(pc, "simulation_paused"):
        pc.action("Pause")
    radar = next(s for s in allies if "레이더함" in prop(s, "ship_name"))
    target = enemies[0]
    pc.select_ship(radar)
    hud.execute_ui_action("RadarSurvey")
    pc.action("Pick_" + str(ships.index(target)))
    check("범위 밖 적 내부 비공개", prop(pc, "selected") == radar)
    target.set_actor_location(radar.get_actor_location() + u.Vector(12000, 0, 0), False, False)
    hud.execute_ui_action("RadarSurvey")
    pc.action("Pick_" + str(ships.index(target)))
    check("적 정찰 시 상세창과 층 열기", prop(pc, "selected") == target and prop(hud, "details_open") and prop(pc, "viewed_deck") == 1)
    yield from frames()
    check("정찰 층 버튼 표시", hud.get_action_center("SurveyDeck_1").x >= 0)
    pc.select_ship(radar)
    check("동일 레이더 충전 중 중복 정찰 차단", not pc.survey_enemy(target) and "충전" in prop(pc, "last_action"))
    ship = next(s for s in allies if prop(s, "ship_name").startswith("전함"))
    systems = prop(ship, "systems")
    defs = prop(prop(systems, "layout"), "slots")
    index = next(i for i, d in enumerate(defs) if prop(d, "facility") == u.OVFacilityType.ENGINE)
    slot = prop(defs[index], "slot_id")
    systems.debug_destroy_slot(slot, u.OVDamageCause.SABOTAGE)
    action = f"FacilityAlert_{ships.index(ship)}_{index}"
    yield from frames()
    center = hud.get_action_center(action)
    check("시설 파괴 버튼 및 히트 영역", center.x >= 0 and str(hud.get_hit_action_at_canvas_point(center)) == action)
    systems.debug_repair_slot(slot)
    yield from frames()
    check("복구 후에도 확인 전 경보 유지", hud.get_action_center(action).x >= 0)
    if os.environ.get("OV_ALERT_MOUSE_TEST") == "1":
        deadline = time.monotonic() + 90
        while hud.get_action_center(action).x >= 0 and time.monotonic() < deadline:
            yield
        results["physical_input"] = hud.get_action_center(action).x < 0
    if not results["physical_input"]:
        hud.execute_ui_action(action)
    check("알림에서 해당 함선·층·시설 추적", prop(pc, "selected") == ship and prop(pc, "follow") and
          prop(pc, "viewed_deck") == prop(defs[index], "deck_id") and prop(pc, "selected_slot") == index)
    yield from frames()
    check("확인한 경보 소거", hud.get_action_center(action).x < 0)
    check("임무 카드 좌측 배치", 0 < hud.get_action_center("RestartScene").x < 330)
    check("재시작 후 초기화", pc.start_play_scenario("Fleet200"))


def tick(dt):
    try:
        next(job)
        return
    except StopIteration:
        results["passed"] = True
    except Exception:
        results.update(passed=False, error=traceback.format_exc())
    (out / "pie.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf8")
    u.unregister_slate_post_tick_callback(handle)
    u.OVDevelopmentEditorLibrary.restore_preview_settings()
    u.SystemLibrary.quit_editor()


job = run()
assert u.OVDevelopmentEditorLibrary.start_preview_window(1280, 740)
handle = u.register_slate_post_tick_callback(tick)
