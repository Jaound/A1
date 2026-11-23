"""PIE API: 실제 편성·교전과 판정 경계 확인. OS 입력을 재현하지 않는다."""
import json
import os
from pathlib import Path
import time
import traceback

import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
run = os.environ.get("OV_FLEET_RUN", time.strftime("local_%Y%m%d_%H%M%S"))
if not run.replace("_", "").isalnum():
    raise ValueError("잘못된 실행 ID")
out = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / "Validation" / f"PlayScenarioFleets_{run}.json"
if out.exists():
    raise FileExistsError(out)
out.parent.mkdir(parents=True, exist_ok=True)
report = {"checks": [], "errors": [], "physical_input": False, "passed": False}
started = time.monotonic()


def prop(obj, name):
    return obj.get_editor_property(name)


def check(name, result, detail=""):
    report["checks"].append({"id": name, "passed": bool(result), "detail": str(detail)})
    if not result:
        raise AssertionError(f"{name}: {detail}")


def faction(ship):
    return prop(prop(ship, "systems"), "faction_id")


def advance(pc, seconds, speed=1):
    for _ in range(int(seconds * 4)):
        pc.step_simulation(.25 / speed)


def verify(pc):
    combat = prop(pc, "combat")
    carrier = prop(pc, "carrier")
    play = pc.get_play_scenario()
    expected_aircraft = {50: 40, 100: 40, 200: 40, 300: 80}
    previous_ship = None
    for budget in (50, 100, 200, 300):
        check(f"FLEET-{budget}-START", pc.start_play_scenario(f"Fleet{budget}"), prop(pc, "last_action"))
        ships = prop(pc, "ships")
        if previous_ship is not None:
            check(f"FLEET-{budget}-OLD-ACTOR-CLEARED", previous_ship not in ships)
        previous_ship = ships[0]
        check(f"FLEET-{budget}-COST", list(prop(play, "initial_costs")) == [budget, budget], prop(play, "initial_costs"))
        check(f"FLEET-{budget}-REGISTERED", len(ships) == len(prop(combat, "ships")))
        caps = []
        actual_costs = []
        for side in (0, 1):
            classes = [prop(combat.get_ship_state(ship), "weapon_tier_cap") for ship in ships if faction(ship) == side]
            caps.append(classes.count(4))
            actual_costs.append(sum({1: 1, 2: 5, 3: 25, 4: 125}[size] for size in classes))
        check(f"FLEET-{budget}-SPAWNED-COST", actual_costs == [budget, budget], actual_costs)
        check(f"FLEET-{budget}-CAPITALS", caps == [max(0,budget // 100 - 1)] * 2, caps)
        aircraft = prop(carrier, "aircraft")
        check(f"FLEET-{budget}-AIRCRAFT", [sum(prop(a,"faction_id") == side for a in aircraft) for side in (0,1)]
              == [expected_aircraft[budget]] * 2, len(aircraft))
        ids = [prop(a, "aircraft_id") for a in aircraft]
        check(f"FLEET-{budget}-AIRCRAFT-IDS", len(ids) == len(set(ids)) and all(prop(a,"carrier") in ships for a in aircraft))
        positions = [ship.get_actor_location() for ship in ships]
        advance(pc, 1)
        check(f"FLEET-{budget}-PAUSED", prop(play, "elapsed_seconds") == 0)
        pc.action("Pause")
        advance(pc, 30)
        distances = [(ship.get_actor_location() - start).length() for ship, start in zip(ships, positions) if faction(ship) == 1]
        check(f"FLEET-{budget}-ENEMY-MOVEMENT", max(distances) > 1000, max(distances))
        check(f"FLEET-{budget}-REAL-HITS", prop(combat, "hit_count") > 0, prop(combat, "hit_count"))
        fired = [sum(prop(mount, "shots_fired") for ship in ships if faction(ship) == side
                     for mount in prop(combat.get_ship_state(ship), "mounts")) for side in (0, 1)]
        check(f"FLEET-{budget}-BOTH-SIDES-FIRE", min(fired) > 0, fired)
        check(f"FLEET-{budget}-BOTH-SIDES-LAUNCH",
              all(any(prop(a, "faction_id") == side and prop(a, "state") != unreal.OVAircraftState.STORED
                      for a in prop(carrier, "aircraft")) for side in (0, 1)))

    # Boundary fixture: send all enemy hulls through the combat subsystem's
    # departure event. This tests the end condition, not the FTL charging flow.
    for enemy in [ship for ship in prop(pc, "ships") if faction(ship) == 1]:
        combat.notify_ftl_departure(enemy)
    advance(pc, 1)
    check("FLEET-DEPARTURE-VICTORY", prop(play, "result") == unreal.OVPlaySceneResult.VICTORY and prop(pc, "simulation_paused"))
    pc.action("RestartScene")
    check("FLEET-RESTART-RESET", str(prop(play, "active_id")) == "Fleet300" and
          prop(play, "elapsed_seconds") == 0 and prop(combat, "shot_count") == 0 and len(prop(combat, "attacks")) == 0)
    for friendly in [ship for ship in prop(pc, "ships") if faction(ship) == 0]:
        combat.notify_ftl_departure(friendly)
    pc.action("Pause")
    advance(pc, 1)
    check("FLEET-DEPARTURE-DEFEAT", prop(play, "result") == unreal.OVPlaySceneResult.DEFEAT)

    clock = []
    for speed in (1, 2, 3):
        check(f"FLEET-CLOCK-{speed}-START", pc.start_play_scenario("Fleet100"))
        for ship in prop(pc, "ships"):
            ship.set_automation(False, False)
            ship.stop_movement()
        pc.action(f"Speed{speed}")
        pc.action("Pause")
        advance(pc, 4, speed)
        clock.append(round(prop(play, "elapsed_seconds"), 4))
    check("FLEET-CLOCK-123", clock == [4.0, 4.0, 4.0], clock)
    advance(pc, 117, 3)
    check("FLEET-STALEMATE", prop(play, "result") == unreal.OVPlaySceneResult.STALEMATE and
          "교착" in play.get_status() and prop(combat, "shot_count") == 0, play.get_status())
    check("FLEET-SANDBOX-START", pc.start_play_scenario("Sandbox"))
    check("FLEET-SANDBOX-EMPTY", not prop(pc, "ships") and not prop(combat, "ships") and not prop(carrier, "aircraft"))
    pc.action("Pause")
    advance(pc, 1)
    check("FLEET-SANDBOX-NO-RESULT", prop(play, "result") == unreal.OVPlaySceneResult.RUNNING)
    pc.leave_play_scenario()
    check("FLEET-LEAVE-RESET", str(prop(play, "active_id")) == "None" and not prop(pc, "selected"))
    check("FLEET-LEGACY-API-RESTORE", pc.start_calibration_range(1) and len(prop(pc, "ships")) == 3)


def tick(_delta):
    try:
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or not prop(pc, "ships"):
            if time.monotonic() - started > 90:
                raise TimeoutError("PIE 시동 시간 초과")
            return
        verify(pc)
        report["passed"] = True
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
    finally:
        if report["passed"] or report["errors"]:
            out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()


level.editor_request_begin_play()
handle = unreal.register_slate_post_tick_callback(tick)
