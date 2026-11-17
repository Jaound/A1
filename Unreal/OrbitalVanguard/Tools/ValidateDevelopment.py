"""DEV PIE API 검사. 실제 마우스/UMG 입력과 최종 밸런스 승인은 검증하지 않는다."""
import collections
import json
import os
import time
import traceback
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = os.path.join(ROOT, "Validation", "Development.json")
os.makedirs(os.path.dirname(OUT), exist_ok=True)
report = {"passed": None, "scope": "DEV PIE API; 물리 입력/실제 게임 밸런스 제외",
          "planning": "035a4fba81d26f1d67daa1f44bded0f7e8d3a434", "checks": [], "errors": [], "cases": []}
state = {"phase": 0, "start": time.monotonic()}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def p(obj, key):
    return obj.get_editor_property(key)


def check(name, value, detail=""):
    report["checks"].append({"id": name, "passed": bool(value), "detail": str(detail)})
    unreal.log("OVDEV %s %s %s" % (name, bool(value), detail))


def save():
    report["passed"] = bool(report["checks"]) and all(x["passed"] for x in report["checks"]) and not report["errors"]
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)


def abort(message):
    raise RuntimeError(message)


def execute(pc):
    pc.set_editor_property("developer_mode", True)
    check("시작 F6", pc.start_calibration_range(1))
    dev, combat = p(pc, "development"), p(pc, "combat")
    pd, carrier = p(pc, "point_defense"), p(pc, "carrier")
    transit, interior = p(pc, "transport"), p(pc, "interior")
    catalogue = unreal.get_default_object(unreal.OVDevelopmentCatalogue)
    definitions = catalogue.get_definitions()
    by_category = collections.Counter(str(p(d, "category")) for d in definitions)
    hulls = [d for d in definitions if p(d, "category") == unreal.OVCatalogCategory.HULL]
    weapons = [d for d in definitions if p(d, "category") == unreal.OVCatalogCategory.WEAPON]
    planned = [d for d in definitions if str(p(d, "definition_id")) == "planned_special_weapon"]
    check("카탈로그 수량과 구분", len(hulls) == 16 and len(weapons) == 26 and len(definitions) == 50 and len(planned) == 1, by_category)
    check("기획만 특수 무장 구분", len(planned) == 1 and p(p(planned[0], "planning"), "status") == unreal.OVCatalogStatus.AVAILABLE
          and p(p(planned[0], "data"), "status") == unreal.OVCatalogStatus.UNAVAILABLE
          and p(p(planned[0], "direct_input"), "status") == unreal.OVCatalogStatus.UNAVAILABLE)
    check("정의 ID 고유성", len({str(p(d, "definition_id")) for d in definitions}) == len(definitions))
    check("검증 근거 상태 구분", all(
        (p(p(d, "automated_verification"), "status") == unreal.OVCatalogStatus.UNKNOWN and not p(p(d, "automated_verification"), "verification_sha")) or
        (p(p(d, "automated_verification"), "status") == unreal.OVCatalogStatus.PARTIAL and len(p(p(d, "automated_verification"), "verification_sha")) == 40)
        for d in definitions))
    check("과거 자동 검증과 직접 입력 분리", all(p(p(d, "direct_input"), "status") == unreal.OVCatalogStatus.UNKNOWN for d in hulls))
    if len(hulls) != 16:
        abort("16 함체 카탈로그 필요")
    ids = [str(p(d, "definition_id")) for d in hulls]
    expected = {"corvette", "frigate", "vector", "bastion", "cruiser", "air_defense", "radar", "battleship", "carrier", "missile_ship", "dreadnought", "battlecarrier", "repair", "mining", "transport", "assault"}
    check("실제 16 정의 목록", set(ids) == expected, ids)
    xs, ys = [-150000, -75000, 75000, 150000], [-150000, -50000, 50000, 150000]
    ships = {}
    for i, hull_id in enumerate(ids):
        position = unreal.Vector(xs[i % 4], ys[i // 4], 2000)
        made = dev.spawn_ships(hull_id, 0, 1, position, 0, 2 if hull_id == "carrier" else 0, 0, False, False)
        ok = len(made) == 1
        check("생성 " + hull_id, ok, p(dev, "last_message"))
        if not ok:
            continue
        ship = made[0]; ships[hull_id] = ship
        systems = p(ship, "systems"); layout = p(systems, "layout")
        mounts = p(layout, "weapon_mounts")
        expected_pd = sum(p(m, "type") == unreal.OVWeaponType.INTERCEPTION for m in mounts)
        state_pd = pd.get_ship_state(ship)
        check("전투 등록 " + hull_id, p(combat.get_ship_state(ship), "ship") == ship)
        check("PD 등록 " + hull_id, expected_pd > 0 and len(p(state_pd, "mounts")) == expected_pd)
        check("선내 그래프 " + hull_id, interior.is_structure_known(0, ship))
        # A newly registered transport ship accepts DEV passengers. Use one later to
        # exercise the removal guard without making every fixture undeletable.
        report["cases"].append({"definition_id": hull_id, "runtime_id": ship.get_name(),
                                "pd_count": expected_pd, "mounts": len(mounts),
                                "hull_hp": p(systems, "max_hull_hp")})
    check("F6 방공 시스템 활성", p(pd, "enabled"), "등록뿐 아니라 실제 Step이 실행돼야 함")
    if len(ships) != 16:
        abort("일부 함체 생성 실패; 이후 검사의 선행 조건 부족")
    corvette = ships["corvette"]
    same_place = corvette.get_actor_location()
    before_count = len(p(dev, "spawned_ships"))
    overlap = dev.spawn_ships("corvette", 0, 1, same_place, 0, 0, 0, False, False)
    check("겹침 생성 거부와 롤백", not overlap and len(p(dev, "spawned_ships")) == before_count)
    carrier_ship = ships["carrier"]
    hangars = [h for h in p(carrier, "hangars") if p(h, "carrier") == carrier_ship]
    craft = [x for x in p(carrier, "aircraft") if p(x, "carrier") == carrier_ship]
    check("항모 격납고와 기체 등록", len(hangars) >= 1 and len(craft) == 2 * len(hangars), (len(hangars), len(craft)))
    # The two extra-large layouts must have the original T3/T4 profiles and true
    # per-family explosion damage. Small base avoids neighbor HP saturation.
    for key, expected_sum in [("dreadnought", 146.0), ("battlecarrier", 86.0)]:
        systems = p(ships[key], "systems"); layout = p(systems, "layout")
        mounts = [m for m in p(layout, "weapon_mounts") if p(m, "type") == unreal.OVWeaponType.CANNON]
        coef = sum(p(m, "explosion_tier_coefficient") for m in mounts)
        check("혼합 티어 계수 " + key, abs(coef - expected_sum) < .001 and {round(p(m, "tier")) for m in mounts} == {3, 4}, coef)
        slots = p(layout, "slots")
        ammo = next((x for x in slots if p(x, "facility") == unreal.OVFacilityType.AMMO), None)
        if not ammo:
            check("유폭 시설 " + key, False); continue
        ammo_id = p(ammo, "slot_id"); neighbors = set()
        for edge in p(layout, "connections"):
            a, b = p(edge, "from_slot"), p(edge, "to_slot")
            if a == ammo_id: neighbors.add(str(b))
            if b == ammo_id: neighbors.add(str(a))
        nonempty = [x for x in slots if str(p(x, "slot_id")) in neighbors and p(x, "facility") != unreal.OVFacilityType.NONE]
        if not nonempty:
            check("유폭 인접 시설 " + key, False); continue
        neighbor_id = p(nonempty[0], "slot_id")
        previous = p(systems.get_slot_state(neighbor_id), "hp")
        tuning = p(systems, "tuning"); tuning.set_editor_property("explosion_base", .01); systems.set_editor_property("tuning", tuning)
        systems.debug_destroy_slot(ammo_id, unreal.OVDamageCause.SABOTAGE)
        dealt = previous - p(systems.get_slot_state(neighbor_id), "hp")
        check("혼합 티어 실제 유폭 피해 " + key, abs(dealt - expected_sum * .01) < .02 and p(systems, "last_explosion_count") == 1, dealt)
    # Re-equipping family and tier rules are enforced by real combat subsystem.
    cmount = next(m for m in p(p(p(corvette, "systems"), "layout"), "weapon_mounts") if p(m, "type") == unreal.OVWeaponType.CANNON)
    mount_id = p(cmount, "weapon_id")
    corvette_systems = p(corvette, "systems")
    corvette_systems.debug_hit_weapon(mount_id, 10)
    health_before = next(p(m, "hp") for m in p(corvette_systems, "weapon_mount_states") if p(m, "weapon_mount_id") == mount_id)
    check("동일 계열·티어 장착", dev.equip_weapon(corvette, mount_id, "D116_heavy_cannon_T1"), p(dev, "last_message"))
    check("타 계열 장착 거부", not dev.equip_weapon(corvette, mount_id, "D116_beam_T1"))
    check("타 티어 장착 거부", not dev.equip_weapon(corvette, mount_id, "D116_cannon_T2"))
    health_after = next(p(m, "hp") for m in p(corvette_systems, "weapon_mount_states") if p(m, "weapon_mount_id") == mount_id)
    check("장착으로 손상 무장 HP 회복 없음", health_before == health_after)
    # Position relative to corvette; outside both hulls but inside guaranteed detection.
    cp = corvette.get_actor_location()
    enemy_list = dev.spawn_ships("corvette", 1, 1, unreal.Vector(cp.x + 20000, cp.y, cp.z), 0, 0, 0, False, False)
    check("적 함체 생성", len(enemy_list) == 1, p(dev, "last_message"))
    if enemy_list:
        enemy = enemy_list[0]
        check("직접 표적 지정", dev.set_ship_target(corvette, enemy))
        check("직접 발사 요청", dev.fire_weapon(corvette, mount_id), p(dev, "last_message"))
        if p(pc, "simulation_paused"): pc.action("Pause")
        flight = None
        for _ in range(400):
            pc.step_simulation(.025)
            attacks = [a for a in p(combat, "attacks") if str(p(a, "attacker_id")) == corvette.get_name()]
            if attacks:
                flight = attacks[0]; break
        if not p(pc, "simulation_paused"): pc.action("Pause")
        check("실제 탄체 발사", flight is not None, p(dev, "last_message"))
        if flight:
            attack_id, faction, attacker = p(flight, "attack_id"), p(flight, "faction_id"), str(p(flight, "attacker_id"))
            pc.select_ship(corvette)
            check("발사 후 원 함체 삭제", dev.delete_ship(corvette), p(dev, "last_message"))
            surviving = [a for a in p(combat, "attacks") if p(a, "attack_id") == attack_id]
            check("기발사 정보 유지", len(surviving) == 1 and p(surviving[0], "faction_id") == faction and str(p(surviving[0], "attacker_id")) == attacker)
            check("선택 삭제 정리", p(pc, "selected") is None)
        observer = ships["frigate"]
        check("다른 함선 표적 설정", dev.set_ship_target(observer, enemy))
        check("표적 삭제", dev.delete_ship(enemy), p(dev, "last_message"))
        check("삭제 표적 참조 정리", p(observer, "attack_target") is None)
    # Explicit transport passengers make deletion unsafe, carrier aircraft likewise.
    token_ship = ships["transport"]
    people = transit.add_dev_passengers(token_ship, 0, 1)
    check("운송 등록 후 승객 생성", len(people) == 1)
    check("승객 있는 함선 삭제 거부", not dev.delete_ship(token_ship))
    launched = carrier.launch(carrier_ship)
    check("항모 실제 발진", launched > 0, launched)
    check("미귀환 기체 함선 삭제 거부", not dev.delete_ship(carrier_ship))


def tick(_dt):
    try:
        if state["phase"] == 0:
            level.editor_request_begin_play(); state["phase"] = 1; return
        world = editor.get_game_world()
        if not world:
            if time.monotonic() - state["start"] > 60: abort("PIE 준비 시간 초과")
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not isinstance(pc, unreal.OVController) or not p(pc, "development"):
            if time.monotonic() - state["start"] > 60: abort("개발 컨트롤러 준비 시간 초과")
            return
        execute(pc)
        save()
        unreal.unregister_slate_post_tick_callback(state["handle"])
    except Exception:
        report["errors"].append(traceback.format_exc()); save()
        unreal.log_error(report["errors"][-1])
        unreal.unregister_slate_post_tick_callback(state["handle"])

state["handle"] = unreal.register_slate_post_tick_callback(tick)
