"""영웅 카드와 실제 영웅/선내 상태의 PIE API 검사. OS 입력·외관 판정은 별도다."""
import json
import os
import time
import traceback
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
out = os.path.join(root, "Validation", "HeroCards.json")
os.makedirs(os.path.dirname(out), exist_ok=True)
report = {"passed": None, "kind": "영웅 카드 PIE API", "checks": [], "errors": [],
          "scope": "카드 선택·실제 위치·상태·수명 API; 실제 마우스/키보드 및 초상화 외관 확인 아님"}
state = {"phase": 0, "started": time.monotonic()}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def p(obj, key):
    return obj.get_editor_property(key)


def guid(value):
    return tuple(p(value, key) for key in ("a", "b", "c", "d"))


def check(name, value, detail=""):
    report["checks"].append({"id": name, "passed": bool(value), "detail": str(detail)})
    unreal.log("OVHEROCARD %s %s %s" % (name, bool(value), detail))
    if not value:
        raise AssertionError(name + ": " + str(detail))


def rows(widget):
    widget.refresh_cards()
    return list(widget.get_card_views())


def roster_snapshot(heroes):
    return [(guid(p(h, "hero_id")), p(h, "xp"), p(h, "level"), p(h, "current_hp"),
             tuple(p(h, "skill_ranks")), p(h, "catalysts"), p(h, "status"),
             p(h, "runtime_unit_id"), p(h, "runtime_person_id")) for h in heroes.get_heroes(0)]


def runtime_snapshot(pc):
    interior, transport, carriers = p(pc, "interior"), p(pc, "transport"), p(pc, "carrier")
    units = [(p(u, "unit_id"), p(u, "person_id"), p(u, "hp"), p(u, "state"),
              p(u, "shots"), p(u, "skill_uses")) for u in p(interior, "units")]
    people = [(p(u, "person_id"), p(u, "runtime_unit_id"), p(u, "state")) for u in p(transport, "persons")]
    # 같은 틱 안 UI 조회 전후의 권위 상태를 비교한다.
    return units, people, len(p(carriers, "aircraft"))


def run(pc, widget):
    heroes, dev, mission = p(pc, "heroes"), p(pc, "development"), p(pc, "mission")
    check("HC-ROOM", dev.open_roster_room(), p(dev, "last_message"))
    roles = [unreal.OVInteriorRole.MELEE, unreal.OVInteriorRole.RANGED,
             unreal.OVInteriorRole.SUPPORT, unreal.OVInteriorRole.COMMANDER]
    ids = []
    for index, role in enumerate(roles):
        identifier = heroes.create_dev_hero(0, role, ["역장 선봉", "정밀 사수", "재생 지원", "전술 지휘"][index])
        ids.append(identifier)
        check("HC-RESERVE-%d" % index, heroes.set_deployment(identifier, True))
    check("HC-XP-FIXTURE", heroes.grant_dev_xp(ids[0], 370))
    initial_roster = roster_snapshot(heroes)
    cards = rows(widget)
    check("HC-FOUR-OWNED", len(cards) == 4 and not any(p(c, "mission_only") for c in cards), len(cards))
    check("HC-OWNED-IDS", {guid(p(c, "hero_id")) for c in cards} == {guid(i) for i in ids})
    check("HC-FOUR-ROLE-PORTRAITS", {p(c, "role_index") for c in cards} == {1, 2, 3, 4})
    check("HC-RESERVE-UNKNOWN-HP", all(not p(c, "hp_known") for c in cards))
    check("HC-OWNED-SELECT", widget.select_hero(ids[2]) and guid(widget.get_selected_hero_id()) == guid(ids[2]))
    check("HC-ONE-SELECTED", sum(bool(p(c, "selected")) for c in rows(widget)) == 1)
    check("HC-RESERVE-NO-FAKE-LOCATION", not widget.locate_selected_hero())
    rebuilds = widget.get_card_rebuild_count()
    for _ in range(100):
        widget.refresh_cards()
    check("HC-REFRESH-NO-REBUILD", widget.get_card_rebuild_count() == rebuilds, rebuilds)
    check("HC-REFRESH-NO-GROWTH-OR-HEAL", initial_roster == roster_snapshot(heroes))

    check("HC-OWNED-MISSION", mission.start_roster_mission(pc, False, 260112, False), p(mission, "status"))
    cards = rows(widget)
    check("HC-NO-TEMPLATE-DUPLICATE", len(cards) == 4 and not any(p(c, "mission_only") for c in cards))
    interior = p(pc, "interior")
    for index, identifier in enumerate(ids):
        hero = heroes.get_hero(identifier)
        unit = interior.get_unit(p(hero, "runtime_unit_id"))
        card = next(c for c in cards if guid(p(c, "hero_id")) == guid(identifier))
        check("HC-LIVE-IDENTITY-%d" % index,
              p(card, "unit_id") == p(unit, "unit_id") and p(card, "unit_id") > 0
              and guid(p(unit, "hero_id")) == guid(identifier))
        check("HC-LIVE-HP-%d" % index,
              p(card, "hp_known") and abs(p(card, "hp") - p(unit, "hp")) < .001
              and abs(p(card, "max_hp") - p(unit, "max_hp")) < .001)
    check("HC-SELECT-LIVE", widget.select_hero(ids[0]))
    unit_id = widget.get_selected_unit_id()
    unit = interior.get_unit(unit_id)
    origin = p(unit, "ship")
    check("HC-LOCATE-LIVE", widget.locate_selected_hero() and p(pc, "selected") == origin and p(pc, "follow"))
    check("HC-LOCATE-FOCUS", (p(pc, "camera_focus") - origin.get_actor_location()).length() < .01)
    before = runtime_snapshot(pc), roster_snapshot(heroes)
    rebuilds = widget.get_card_rebuild_count()
    for index in range(80):
        widget.set_visibility(unreal.SlateVisibility.COLLAPSED if index % 2 else unreal.SlateVisibility.VISIBLE)
        widget.select_hero(ids[index % 4])
        widget.refresh_cards()
    check("HC-80-SELECTIONS-NO-REBUILD", widget.get_card_rebuild_count() == rebuilds)
    check("HC-UI-DOES-NOT-SIMULATE", before == (runtime_snapshot(pc), roster_snapshot(heroes)))
    slots = list(p(p(p(origin, "systems"), "layout"), "slots"))
    goal = next(p(s, "slot_id") for s in slots if p(s, "slot_id") != p(unit, "node_id"))
    check("HC-MOVEMENT-ORDER", interior.issue_order([unit_id], unreal.OVInteriorOrder.DEFEND, goal))
    interior.simulate(.1)
    widget.select_hero(ids[0])
    current = interior.get_unit(unit_id)
    card = next(c for c in rows(widget) if p(c, "selected"))
    check("HC-ACTUAL-MOVEMENT-STATE", p(current, "distance_travelled") > 0 and p(card, "state") in ("이동", "수호"), p(card, "state"))
    check("HC-LOCATION-NO-INTERNAL-NODE-ID", str(p(current, "node_id")) not in p(card, "location"), p(card, "location"))

    # 전장 시계를 진행하지 않은 명시적 장면 교체 fixture. 보유 명단은 유지되고 예시 인원만 별도 표시한다.
    check("HC-EXAMPLE-MISSION", mission.start_mission(pc, unreal.OVMissionMode.ATTACK, 260112, 4, 2, False))
    cards = rows(widget)
    examples = [c for c in cards if p(c, "mission_only")]
    owned = [c for c in cards if not p(c, "mission_only")]
    check("HC-EXAMPLES-SEPARATE", len(examples) == 4 and len(owned) == 4)
    check("HC-EXAMPLES-NO-ROSTER-GRANT", len(heroes.get_heroes(0)) == 4)
    check("HC-EXAMPLE-SELECTION", widget.select_mission_hero(p(examples[0], "unit_id")))
    check("HC-EXAMPLE-LOCATE", widget.locate_selected_hero())
    enemies = [u for u in p(interior, "units") if p(u, "faction_id") != 0]
    troops = [u for u in p(interior, "units") if p(u, "faction_id") == 0 and p(u, "role") == unreal.OVInteriorRole.TROOPER]
    check("HC-ENEMY-NOT-DISCLOSED", not {p(u, "unit_id") for u in enemies}.intersection({p(c, "unit_id") for c in examples}))
    check("HC-ENEMY-NOT-SELECTABLE", bool(enemies) and not widget.select_mission_hero(p(enemies[0], "unit_id")))
    check("HC-TROOPS-NOT-HERO-CARDS", bool(troops) and not widget.select_mission_hero(p(troops[0], "unit_id")))
    check("HC-INVALID-ID-REJECT", not widget.select_mission_hero(999999))
    # 같은 방의 실제 적 전투원으로 자동 스킬을 발동시킨다. UI가 쿨타임을 직접 변경하지 않는다.
    focus_id = p(examples[0], "unit_id")
    focus = interior.get_unit(focus_id)
    example_origin = p(focus, "ship")
    enemy_id = interior.register_unit(example_origin, 1, unreal.OVInteriorRole.MELEE, p(focus, "node_id"))
    check("HC-COMBAT-FIXTURE", enemy_id > 0)
    interior.simulate(.1)
    widget.select_mission_hero(focus_id)
    focus = interior.get_unit(focus_id)
    remaining = max(0.0, p(focus, "skill_ready_at") - p(interior, "game_time"))
    skill_labels = list(widget.get_selected_skill_labels())
    check("HC-ACTUAL-SKILL-COOLDOWN", remaining > 0 and ("%.1f초" % remaining) in skill_labels[0], skill_labels)
    check("HC-SKILL-NAME", "역장 방어" in skill_labels[0])
    for _ in range(30):
        widget.refresh_cards()
    check("HC-PAUSED-UI-NO-COOLDOWN-TICK", list(widget.get_selected_skill_labels()) == skill_labels)
    interior.simulate(.2)
    widget.refresh_cards()
    next_remaining = max(0.0, p(interior.get_unit(focus_id), "skill_ready_at") - p(interior, "game_time"))
    check("HC-COOLDOWN-GAME-CLOCK", next_remaining < remaining and ("%.1f초" % next_remaining) in widget.get_selected_skill_labels()[0])

    # 실제 피해로 쉴드를 제거하는 명시적 운송 fixture. 비행 중 위치 조회는 같은 개인의 원 포드만 따른다.
    transport, target = p(pc, "transport"), p(mission, "objective")
    systems = p(target, "systems")
    systems.apply_world_hit(target.get_actor_location() + target.get_actor_forward_vector() * 100000,
                            target.get_actor_forward_vector(), p(systems, "shield") + 1,
                            unreal.OVDamageCause.EXTERNAL, True)
    check("HC-POD-QUEUE", transport.launch_pods(example_origin, target) > 0)
    transport.simulate(.1)
    interior.simulate(.1)
    widget.refresh_cards()
    person = transport.get_person(p(interior.get_unit(focus_id), "person_id"))
    check("HC-ACTUAL-POD-TRANSIT", p(person, "state") == unreal.OVPassengerState.IN_POD)
    pod = next(item for item in p(transport, "pods") if p(item, "pod_id") == p(person, "pod_id"))
    check("HC-POD-PERSON-LINK", p(pod, "person_id") == p(person, "person_id"))
    check("HC-POD-LOCATION", widget.locate_selected_hero() and not p(pc, "follow")
          and (p(pc, "camera_focus") - p(pod, "position")).length() < .01)
    transit_card = next(c for c in rows(widget) if p(c, "selected"))
    check("HC-POD-NOT-INTERIOR-LOCATION", p(transit_card, "state") == "운송" and p(transit_card, "location") == "포드 운송")
    check("HC-REAL-POD-LOSS", transport.intercept_pod(p(pod, "pod_id"), p(pod, "hp") + 1))
    interior.simulate(.1)
    rescue_card = next(c for c in rows(widget) if p(c, "selected"))
    check("HC-RESCUE-NOT-DEATH-OR-RETURN", p(rescue_card, "state") == "구조 대기" and not p(rescue_card, "lost")
          and p(interior.get_unit(focus_id), "state") == unreal.OVInteriorState.RESCUE_WAITING)
    check("HC-RESCUE-NO-FAKE-LOCATION", not widget.locate_selected_hero())
    report["counts"] = {"owned": len(owned), "mission_only": len(examples), "card_rebuilds": widget.get_card_rebuild_count()}


def finish():
    report["passed"] = bool(report["checks"]) and all(row["passed"] for row in report["checks"]) and not report["errors"]
    report["elapsed_seconds"] = time.monotonic() - state["started"]
    with open(out, "w", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state["handle"])


def tick(_dt):
    try:
        if time.monotonic() - state["started"] > 90:
            raise RuntimeError("영웅 카드 PIE 준비 시간 초과")
        if state["phase"] == 0:
            level.editor_request_begin_play()
            state["phase"] = 1
            return
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or not pc.get_hud() or not pc.get_hud().get_hero_panel():
            return
        run(pc, pc.get_hud().get_hero_panel())
        finish()
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
