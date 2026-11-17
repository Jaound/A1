"""실제 포탑·발사 원점·읽기 전용 프리뷰 PIE API 회귀. 물리 입력 검사가 아니다."""
import json
import os
import time
import traceback
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
out = os.path.join(root, "Validation", "WeaponPresentation.json")
os.makedirs(os.path.dirname(out), exist_ok=True)
report = {"passed": None, "kind": "포탑 표시 PIE API", "physical_input": False,
          "scope": "임시 가동 포탑, 실제 발사 경로, 읽기 전용 프리뷰; 최종 메시/직접 입력 제외",
          "checks": [], "errors": [], "speed_samples": []}
state = {"phase": "start", "started": time.monotonic()}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def p(obj, key):
    return obj.get_editor_property(key)


def check(key, value, detail=""):
    report["checks"].append({"id": key, "passed": bool(value), "detail": str(detail)})
    unreal.log("OVWVP %s %s %s" % (key, bool(value), detail))
    if not value:
        raise AssertionError(key + ": " + str(detail))


def distance(a, b):
    return (a - b).length()


def angles_equal(a, b, tolerance=.001):
    return all(abs((getattr(a, axis) - getattr(b, axis) + 180) % 360 - 180) < tolerance
               for axis in ("pitch", "yaw", "roll"))


def mount(combat, ship, mount_id):
    return next(m for m in p(combat.get_ship_state(ship), "mounts") if str(p(m, "mount_id")) == mount_id)


def visual(ship, mount_id):
    return next(v for v in ship.get_weapon_visual_states() if str(p(v, "mount_id")) == mount_id)


def fixture(pc):
    pc.start_combat_range()
    ships = list(p(pc, "ships"))
    source, target, bystander = ships[1], ships[2], ships[0]
    for ship in ships:
        ship.set_automation(False, False)
        ship.stop_movement()
    source.set_actor_location(unreal.Vector(0, 0, 0), False, False)
    target.set_actor_location(unreal.Vector(4000, 1500, 700), False, False)
    bystander.set_actor_location(unreal.Vector(-18000, 10000, 0), False, False)
    pc.select_ship(source)
    combat = p(pc, "combat")
    check("WVP-TARGET-REQUEST", combat.set_target(source, target))
    return combat, source, target


def run(pc):
    combat, source, target = fixture(pc)
    check("WVP-REGISTERED", len(source.get_weapon_visual_states()) == len(p(combat.get_ship_state(source), "mounts")))
    beam = "Weapon_1"
    before = p(mount(combat, source, beam), "local_aim")
    combat.simulate(.1)
    current = mount(combat, source, beam)
    view = visual(source, beam)
    check("WVP-REAL-ROTATION", not angles_equal(before, p(current, "local_aim")))
    check("WVP-NO-SECOND-INTERPOLATION", angles_equal(p(view, "visual_aim"), p(current, "local_aim")))
    check("WVP-MUZZLE-QUERY", distance(p(view, "world_muzzle"), source.get_weapon_muzzle_world(beam)) < .01)
    original = p(current, "local_aim")
    game_time = p(combat, "game_time")
    combat.simulate(0)
    source.refresh_weapon_visuals()
    check("WVP-PAUSE-AIM", angles_equal(original, p(visual(source, beam), "visual_aim")))
    check("WVP-PAUSE-CLOCK", p(combat, "game_time") == game_time)
    combat.simulate(2)
    readiness = combat.get_readiness(source, beam)
    check("WVP-ALIGNED-READY", p(readiness, "ready"), p(readiness, "reason"))
    shots = p(combat, "shot_count")
    check("WVP-FIRE-REQUEST", combat.request_fire(source, beam))
    combat.simulate(1 / 120)
    check("WVP-SINGLE-SHOT", p(combat, "shot_count") == shots + 1)
    event = list(p(combat, "recent_events"))[-1]
    view = visual(source, beam)
    check("WVP-REAL-EVENT-ID", p(view, "last_shot_id") == p(event, "attack_id"))
    check("WVP-REAL-MUZZLE", distance(p(view, "world_muzzle"), p(event, "origin")) < .01)
    check("WVP-FLASH", p(view, "flash_visible"))
    for _ in range(50):
        source.refresh_weapon_visuals()
    check("WVP-NO-REPLAY", p(combat, "shot_count") == shots + 1)
    combat.simulate(.2)
    check("WVP-FLASH-GAME-TIME", not p(visual(source, beam), "flash_visible"))
    check("WVP-VLS-FIXED", p(visual(source, "Weapon_2"), "fixed_vls")
          and abs(p(visual(source, "Weapon_2"), "visual_aim").pitch - 90) < .001)
    check("WVP-LOWER-MOUNT", p(visual(source, "Weapon_3"), "bottom"))
    systems = p(source, "systems")
    systems.debug_hit_weapon(beam, 100000)
    old_angle = p(mount(combat, source, beam), "local_aim")
    target.set_actor_location(unreal.Vector(-3500, 500, 600), False, False)
    combat.simulate(.2)
    check("WVP-DESTROYED-TRACKING", p(visual(source, beam), "destroyed")
          and angles_equal(old_angle, p(mount(combat, source, beam), "local_aim")))
    check("WVP-DESTROYED-FIRE-GATE", not p(combat.get_readiness(source, beam), "ready"))
    systems.debug_repair_weapon(beam)
    combat.simulate(.2)
    check("WVP-REPAIRED-TRACKING", not p(visual(source, beam), "destroyed")
          and not angles_equal(old_angle, p(mount(combat, source, beam), "local_aim")))

    # Equal game seconds through the real controller speed path, using the same deterministic fixture.
    for speed in (1, 2, 3):
        combat, source, target = fixture(pc)
        source.set_automation(False, True)
        pc.action("Speed%d" % speed)
        if p(pc, "simulation_paused"):
            pc.action("Pause")
        for _ in range(120):
            pc.step_simulation((4 / speed) / 120)
        pc.action("Pause")
        sample = {"speed": speed, "game_seconds": p(combat, "game_time"), "shots": p(combat, "shot_count"),
                  "hits": p(combat, "hit_count"), "target_hp": p(p(target, "systems"), "hull_hp"),
                  "aim": [p(visual(source, beam), "visual_aim").pitch, p(visual(source, beam), "visual_aim").yaw]}
        report["speed_samples"].append(sample)
        check("WVP-SPEED-%d-POSE" % speed, angles_equal(p(visual(source, beam), "visual_aim"), p(mount(combat, source, beam), "local_aim")))
    a = report["speed_samples"][0]
    for b in report["speed_samples"][1:]:
        check("WVP-SPEED-%d-EQUIVALENT" % b["speed"], abs(a["game_seconds"] - b["game_seconds"]) < .01
              and a["shots"] == b["shots"] and a["hits"] == b["hits"] and abs(a["target_hp"] - b["target_hp"]) < .01)
    if not p(pc, "simulation_paused"):
        pc.action("Pause")
    pc.action("Speed1")
    pc.select_ship(source)
    pc.get_hud().set_details_open(True)
    state.update(phase="preview", at=time.monotonic(), source=source, combat=combat,
                 snapshot=(p(combat, "shot_count"), p(p(source, "systems"), "hull_hp"),
                           len(p(p(pc, "carrier"), "aircraft")), len(unreal.GameplayStatics.get_all_actors_of_class(editor.get_game_world(), unreal.OVShip))))


def finish():
    report["passed"] = bool(report["checks"]) and all(c["passed"] for c in report["checks"]) and not report["errors"]
    with open(out, "w", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state["handle"])


def tick(_dt):
    try:
        if time.monotonic() - state["started"] > 90:
            raise RuntimeError("포탑 PIE 검사 시간 초과")
        if state["phase"] == "start":
            level.editor_request_begin_play()
            state["phase"] = "wait"
            return
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or len(p(pc, "ships")) < 3:
            return
        if state["phase"] == "wait":
            run(pc)
            return
        if state["phase"] == "preview" and time.monotonic() - state["at"] > 1:
            source, combat, hud = state["source"], state["combat"], pc.get_hud()
            check("WVP-PREVIEW-REAL-MESH", hud.get_preview_mesh_count() > 0, hud.get_preview_mesh_count())
            current = (p(combat, "shot_count"), p(p(source, "systems"), "hull_hp"),
                       len(p(p(pc, "carrier"), "aircraft")), len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.OVShip)))
            check("WVP-PREVIEW-READ-ONLY", current == state["snapshot"], (state["snapshot"], current))
            finish()
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
