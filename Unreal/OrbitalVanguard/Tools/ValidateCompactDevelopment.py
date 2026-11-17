"""간소화 개발창 공개 명령/조회 회귀. OS 마우스 입력·외관 판정과 구분한다."""
import json
import os
import time
import traceback
import unreal

out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "Validation", "CompactDevelopment.json")
report = {"passed": None, "kind": "간소화 개발창 PIE API", "checks": [], "errors": [],
          "scope": "기존 생성/필터/명단 규칙을 사용하는 공개 UI 명령. 직접 입력 검증 아님"}
state = {"started": time.monotonic(), "phase": 0}


def prop(obj, key):
    return obj.get_editor_property(key)


def check(identifier, condition, detail=""):
    report["checks"].append({"id": identifier, "passed": bool(condition), "detail": str(detail)})
    unreal.log("OVCOMPACT %s %s" % (identifier, condition))
    if not condition:
        raise AssertionError(identifier + ": " + str(detail))


def roster_snapshot(heroes):
    return [(tuple(prop(prop(h, "hero_id"), p) for p in ("a", "b", "c", "d")), prop(h, "xp"), prop(h, "current_hp"),
             str(prop(h, "status")), prop(h, "catalysts")) for h in heroes.get_heroes(0)]


def run(pc, widget):
    dev = prop(pc, "development")
    heroes = prop(pc, "heroes")
    pc.set_editor_property("developer_mode", True)
    check("CDEV-DEFAULT-SHIP-TAB", widget.get_current_page() == 1)
    check("CDEV-DEFAULT-NO-ADVANCED", not widget.is_advanced_diagnostics())
    check("CDEV-SCENE", pc.start_calibration_range(0))
    check("CDEV-SHIP-PAGE", widget.open_page(1))
    widget.select_item(unreal.Name("vector"))
    compact = widget.get_compact_selection_text()
    check("CDEV-SHORT-NO-INTERNAL-ID", "vector" not in compact and "SHA" not in compact and
          "검증" not in compact and len(compact) < 100, compact)
    before_ids = [str(i) for i in widget.get_visible_definition_ids()]
    check("CDEV-16-HULL-CARDS", len(before_ids) == 16 and len(set(before_ids)) == 16)
    before_ship_count = len(prop(pc, "ships"))
    widget.execute_selected_creation()
    selected = prop(pc, "selected")
    check("CDEV-UI-CREATE-ONE", len(prop(pc, "ships")) == before_ship_count + 1, prop(dev, "last_message"))
    check("CDEV-CREATE-SELECTED", selected in prop(dev, "spawned_ships"))
    check("CDEV-CREATE-FOLLOW", prop(pc, "follow"))
    hp = prop(prop(selected, "systems"), "hull_hp")
    before_shots = prop(prop(pc, "combat"), "shot_count")
    for _ in range(12):
        widget.set_advanced_diagnostics(True)
        widget.select_item(unreal.Name("vector"))
        check("CDEV-ADVANCED-EVIDENCE-%02d" % _, "vector" in widget.get_compact_selection_text())
        widget.set_advanced_diagnostics(False)
    check("CDEV-TOGGLE-FILTER-STABLE", [str(i) for i in widget.get_visible_definition_ids()] == before_ids)
    check("CDEV-TOGGLE-NO-HP", prop(prop(selected, "systems"), "hull_hp") == hp)
    check("CDEV-TOGGLE-NO-FIRE", prop(prop(pc, "combat"), "shot_count") == before_shots)
    check("CDEV-OVERVIEW-API", widget.open_page(0))
    check("CDEV-HIDDEN-DIAGNOSTIC-FILTER", widget.set_filters(4, unreal.Name("None"), -1, -1))
    check("CDEV-XL-FILTER", set(str(i) for i in widget.get_visible_definition_ids()) == {"dreadnought", "battlecarrier"})
    check("CDEV-RESET-FILTER", widget.set_filters(0, unreal.Name("None"), -1, -1))
    check("CDEV-DELETE-REAL", dev.delete_ship(selected), prop(dev, "last_message"))
    check("CDEV-ROSTER-ROOM", dev.open_roster_room(), prop(dev, "last_message"))
    check("CDEV-HERO-PAGE", widget.open_page(2))
    old_count = len(heroes.get_heroes(0))
    widget.select_item(unreal.Name("Role_3"))
    widget.execute_selected_creation()
    records = heroes.get_heroes(0)
    check("CDEV-HERO-CREATE-ONE", len(records) == old_count + 1, prop(heroes, "last_message"))
    check("CDEV-HERO-ROLE", prop(records[-1], "role") == unreal.OVInteriorRole.SUPPORT)
    saved = roster_snapshot(heroes)
    for _ in range(24):
        widget.open_page(_ % 4)
        widget.set_advanced_diagnostics(_ % 2 == 0)
    check("CDEV-24-PAGES-NO-ROSTER-MUTATION", roster_snapshot(heroes) == saved,
          {"before": saved, "after": roster_snapshot(heroes)})
    widget.set_advanced_diagnostics(False)
    widget.open_page(2)
    widget.select_item(unreal.Name(str(widget.get_selected_definition_id())))
    check("CDEV-HERO-NO-ID-WALL", "개인 ID" not in widget.get_compact_selection_text())
    check("CDEV-INVALID-PAGE", not widget.open_page(4))


def finish():
    report["passed"] = bool(report["checks"]) and all(c["passed"] for c in report["checks"]) and not report["errors"]
    report["elapsed_seconds"] = time.monotonic() - state["started"]
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state["handle"])


def tick(_dt):
    try:
        if time.monotonic() - state["started"] > 90:
            raise RuntimeError("PIE/개발창 준비 시간 초과")
        if state["phase"] == 0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            state["phase"] = 1
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or not pc.get_hud() or not pc.get_hud().get_development_panel():
            return
        run(pc, pc.get_hud().get_development_panel())
        finish()
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
