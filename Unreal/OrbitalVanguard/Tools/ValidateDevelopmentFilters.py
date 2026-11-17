"""카탈로그 메타데이터와 UI 필터의 PIE API 대조. 실제 마우스 입력 검증은 아니다.

RunValidation.py의 OV_VALIDATOR로 실행한다. 월드 액터를 생성하거나 제거하지
않으며, 결과는 Saved/Validation/DevelopmentFilters.json에 기록한다.
"""
import json
import os
import time
import traceback
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
out = os.path.join(root, "Validation", "DevelopmentFilters.json")
os.makedirs(os.path.dirname(out), exist_ok=True)
report = {"passed": None, "kind": "개발 카탈로그 필터 PIE API",
          "scope": "공개 UI API와 카탈로그 대조; 실제 마우스·키보드 조작 및 외관 검증 아님",
          "checks": [], "errors": []}
state = {"phase": 0, "started": time.monotonic()}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
none = unreal.Name("None")
axes = ("planning", "data", "generation", "role", "automated_verification", "direct_input")
statuses = (unreal.OVCatalogStatus.UNKNOWN, unreal.OVCatalogStatus.AVAILABLE,
            unreal.OVCatalogStatus.PARTIAL, unreal.OVCatalogStatus.UNAVAILABLE,
            unreal.OVCatalogStatus.NOT_APPLICABLE)


def prop(obj, key):
    return obj.get_editor_property(key)


def check(identifier, value, detail=""):
    report["checks"].append({"id": identifier, "passed": bool(value), "detail": str(detail)})
    unreal.log("OVFILTER %s %s %s" % (identifier, bool(value), detail))
    if not value:
        raise AssertionError(identifier + ": " + str(detail))


def finish():
    report["passed"] = bool(report["checks"]) and all(row["passed"] for row in report["checks"]) and not report["errors"]
    report["elapsed_seconds"] = time.monotonic() - state["started"]
    with open(out, "w", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state["handle"])


def name(definition):
    return str(prop(definition, "definition_id"))


def ids(widget):
    return [str(item) for item in widget.get_visible_definition_ids()]


def snapshot(world, pc):
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.OVShip)
    actor_rows = sorted((actor.get_path_name(), float(prop(prop(actor, "systems"), "hull_hp"))) for actor in actors)
    roster = prop(prop(pc, "heroes"), "heroes")
    hero_rows = sorted((tuple(prop(prop(hero, "hero_id"), part) for part in ("a", "b", "c", "d")), str(prop(hero, "status")),
                        int(prop(hero, "xp")), int(prop(hero, "level")), float(prop(hero, "current_hp")),
                        int(prop(hero, "runtime_unit_id")), int(prop(hero, "runtime_person_id")),
                        tuple(prop(hero, "skill_ranks")), int(prop(hero, "catalysts"))) for hero in roster)
    return actor_rows, hero_rows


def run(world, pc, widget):
    # 빈 명단의 동일성만 확인하지 않도록 실제 성장값이 있는 보유 인원을 준비한다.
    dev = prop(pc, "development")
    check("FILT-ROSTER-ROOM", dev.open_roster_room(), prop(dev, "last_message"))
    heroes = prop(pc, "heroes")
    hero_id = heroes.create_dev_hero(0, unreal.OVInteriorRole.COMMANDER, "필터 수명 검사 영웅")
    check("FILT-ROSTER-XP-FIXTURE", heroes.grant_dev_xp(hero_id, 370), prop(heroes, "last_message"))
    check("FILT-PAGE-OVERVIEW", widget.open_page(0))
    catalogue = unreal.get_default_object(unreal.OVDevelopmentCatalogue)
    definitions = catalogue.get_definitions()
    report["historical_partial_hull_roles"] = [name(d) for d in definitions if
        prop(d, "category") == unreal.OVCatalogCategory.HULL and
        prop(prop(d, "role"), "status") == unreal.OVCatalogStatus.PARTIAL]
    all_ids = [name(d) for d in definitions]
    check("FILT-CATALOGUE-UNIQUE", len(all_ids) == len(set(all_ids)), len(all_ids))
    check("FILT-RESET", widget.set_filters(0, none, -1, -1))
    baseline = ids(widget)
    check("FILT-ALL-EXACT", len(baseline) == len(set(baseline)) and set(baseline) == set(all_ids), len(baseline))
    before = snapshot(world, pc)

    hull4 = [d for d in definitions if prop(d, "size_class") == 4]
    expected4 = {name(d) for d in hull4}
    check("FILT-XL-METADATA", expected4 == {"dreadnought", "battlecarrier"} and
          all(prop(d, "fleet_cost") == 125 for d in hull4), sorted(expected4))
    check("FILT-XL-APPLY", widget.set_filters(4, none, -1, -1))
    check("FILT-XL-EXACT", set(ids(widget)) == expected4 and len(ids(widget)) == 2, ids(widget))

    carrier_role = unreal.Name("함재기 운용")
    role_expected = {name(d) for d in definitions if carrier_role in prop(d, "role_tags")}
    check("FILT-CARRIER-METADATA", role_expected == {"carrier", "battlecarrier"}, sorted(role_expected))
    check("FILT-CARRIER-APPLY", widget.set_filters(0, carrier_role, -1, -1))
    check("FILT-CARRIER-EXACT", set(ids(widget)) == role_expected and len(ids(widget)) == 2, ids(widget))
    check("FILT-COMBINED-APPLY", widget.set_filters(4, carrier_role, -1, -1))
    check("FILT-COMBINED-EXACT", set(ids(widget)) == expected4.intersection(role_expected) == {"battlecarrier"}, ids(widget))

    # Compare every axis/status combination independently to catalogue records.
    for axis_index, axis_name in enumerate(axes):
        for status_index, status in enumerate(statuses):
            expected = {name(d) for d in definitions if prop(prop(d, axis_name), "status") == status}
            identifier = "FILT-AXIS-%d-STATUS-%d" % (axis_index, status_index)
            check(identifier + "-APPLY", widget.set_filters(0, none, axis_index, status_index))
            actual = ids(widget)
            check(identifier + "-EXACT", len(actual) == len(set(actual)) and set(actual) == expected,
                  "actual=%d expected=%d" % (len(actual), len(expected)))

    planned = "planned_special_weapon"
    check("FILT-PLANNED-DATA-APPLY", widget.set_filters(0, none, 1, 3))
    check("FILT-PLANNED-DATA-UNAVAILABLE", planned in ids(widget))
    check("FILT-PLANNED-DIRECT-APPLY", widget.set_filters(0, none, 5, 3))
    check("FILT-PLANNED-DIRECT-UNAVAILABLE", planned in ids(widget))
    check("FILT-PLANNED-PLANNING-APPLY", widget.set_filters(0, none, 0, 1))
    check("FILT-PLANNED-PLANNING-AVAILABLE", planned in ids(widget))
    check("FILT-PLANNED-ROLE-APPLY", widget.set_filters(0, none, 3, 0))
    check("FILT-PLANNED-ROLE-UNKNOWN", planned in ids(widget))

    check("FILT-INVALID-BASE", widget.set_filters(4, carrier_role, -1, -1))
    original = ids(widget)
    for label, size, role, axis, value in (
            ("SIZE", 5, none, -1, -1), ("ROLE", 0, unreal.Name("없는 역할"), -1, -1),
            ("AXIS", 0, none, 6, 0), ("STATUS", 0, none, 1, 5),
            ("NO-AXIS", 0, none, -1, 1)):
        check("FILT-INVALID-" + label + "-REJECT", not widget.set_filters(size, role, axis, value))
        check("FILT-INVALID-" + label + "-PRESERVE", ids(widget) == original)

    repeated = True
    for index in range(30):
        repeated = repeated and widget.set_filters(4, none, 2, -1) and set(ids(widget)) == expected4
        repeated = repeated and widget.set_filters(0, none, -1, -1) and set(ids(widget)) == set(all_ids)
    check("FILT-30-REPEAT-AND-RESET", repeated)
    after = snapshot(world, pc)
    check("FILT-NO-ACTOR-OR-HP-CHANGE", before[0] == after[0], (len(before[0]), len(after[0])))
    check("FILT-NO-ROSTER-CHANGE", before[1] == after[1], (len(before[1]), len(after[1])))


def tick(_dt):
    try:
        if time.monotonic() - state["started"] > 60:
            raise RuntimeError("PIE 또는 UI 준비 시간 초과")
        if state["phase"] == 0:
            level.editor_request_begin_play()
            state["phase"] = 1
            return
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or not pc.get_hud() or not pc.get_hud().get_development_panel():
            return
        pc.set_editor_property("developer_mode", True)
        run(world, pc, pc.get_hud().get_development_panel())
        finish()
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
