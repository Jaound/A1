"""Standalone 2C acceptance in Editor PIE; never injects OS mouse/keyboard input.

Run through UnrealEditor -ExecCmds="py <this file>" on ControlRange.
Writes Saved/Validation/Phase2C.json, separate from the unchanged 2A57/D11679.
Fixture mutations are labelled DEV; actual flight, firing and arrivals use the
production shared controller clock. Leaves a fresh mixed scene paused.
"""
import collections
import json
import math
import os
import statistics
import time
import traceback
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
os.makedirs(os.path.join(root, "Validation"), exist_ok=True)
report = {
    "kind": "Editor PIE API and rendered frames; not physical mouse/keyboard input",
    "planning": "035a4fba81d26f1d67daa1f44bded0f7e8d3a434", "seed": 260112,
    "scope": "Play quality and 2C, independent of existing regression totals",
    "physical_input": "not performed by this script", "checks": [], "errors": [],
    "performance": {"gpu_utilization": "not measured", "cpu_utilization": "not measured",
                    "interpretation": "API batch wall time is not rendered frame time or whole-machine CPU load"},
}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {"phase": 0, "start": time.monotonic()}


def p(obj, key):
    # Digit/acronym boundaries are not uniformly snake-cased by UE's Python bridge.
    if key == "phase2_c_scenario":
        try:
            return obj.get_editor_property("phase2c_scenario")
        except Exception:
            return obj.get_editor_property("Phase2CScenario")
    return obj.get_editor_property(key)


def check(identifier, passed, detail=""):
    report["checks"].append({"id": identifier, "passed": bool(passed), "detail": str(detail)})
    unreal.log("OV2C CHECK " + identifier + " = " + str(bool(passed)) + " " + str(detail))
    if not passed:
        raise AssertionError(identifier + ": " + str(detail))


def save():
    report["passed"] = bool(report["checks"]) and all(c["passed"] for c in report["checks"]) and not report["errors"]
    with open(os.path.join(root, "Validation", "Phase2C.json"), "w", encoding="utf-8") as out:
        json.dump(report, out, ensure_ascii=False, indent=2)


def pause(pc, value):
    if p(pc, "simulation_paused") != value:
        pc.action("Pause")


def advance(pc, seconds, speed=1, batch=.25):
    pc.action("Speed" + str(speed))
    pause(pc, False)
    steps = max(1, math.ceil(seconds / batch))
    for _ in range(steps):
        pc.step_simulation(seconds / steps / speed)
    pause(pc, True)


def subsystem(world, cls):
    # UE 5.8's Python module does not expose get_world_subsystem. Controller
    # owns reflected read-only references to the same production world systems.
    keys = {unreal.OVCarrierSubsystem: "carrier", unreal.OVPointDefenseSubsystem: "point_defense",
            unreal.OVTransportSubsystem: "transport"}
    return p(unreal.GameplayStatics.get_player_controller(world, 0), keys[cls])


def request_scene(pc, scenario):
    for name in ("start_phase2c_range", "start_phase2_c_range"):
        method = getattr(pc, name, None)
        if method is not None:
            return method(scenario)
    raise AttributeError("OVController StartPhase2CRange reflection method missing")


def start(pc, scenario):
    if not request_scene(pc, scenario):
        raise AssertionError("Could not create 2C scene " + str(scenario))
    return list(p(pc, "ships"))


def quiet(ships, combat, pd, carrier):
    """DEV fixture: isolates transport/inventory without changing their rules."""
    pd.disable()
    for ship in ships:
        ship.set_automation(False, False)
        combat.cancel_attack_order(ship)
    for ship in ships:
        if p(p(ship, "systems"), "faction_id") != 0:
            facility(ship, unreal.OVFacilityType.FIGHTER_CONTROL, False)


def facility(ship, kind, enabled):
    systems = p(ship, "systems")
    for slot in p(p(systems, "layout"), "slots"):
        if p(slot, "facility") == kind:
            systems.debug_set_crew(p(slot, "slot_id"), p(slot, "required_crew") if enabled else 0)


def craft_for(carrier, ship):
    return [c for c in p(carrier, "aircraft") if p(c, "carrier") == ship]


def counts(items, key="state"):
    return dict(sorted(collections.Counter(str(p(x, key)) for x in items).items()))


def snapshot(pc, carrier, pd, transport):
    combat = p(pc, "combat")
    return {
        "time": round(p(combat, "game_time"), 4),
        "shots": p(combat, "shot_count"), "hits": p(combat, "hit_count"),
        "flying": len(p(combat, "attacks")),
        "pd": [p(pd, "shot_count"), p(pd, "hit_count"), p(pd, "kill_count")],
        "aircraft_fire": [p(carrier, "shot_count"), p(carrier, "hit_count"), p(carrier, "loss_count")],
        "aircraft": counts(p(carrier, "aircraft")), "bolts": len(p(carrier, "bolts")),
        "pods": counts(p(transport, "pods")), "people": counts(p(transport, "persons")),
        "entries": len(p(transport, "entry_events")),
        "ship_hp": [round(p(p(s, "systems"), "hull_hp"), 3) for s in p(pc, "ships")],
        "shields": [round(p(p(s, "systems"), "shield"), 3) for s in p(pc, "ships")],
    }


def battle(pc, carrier, pd, transport, scenario, speed, seconds=18, batch=.25):
    ships = start(pc, scenario)
    for ship in ships:
        ship.set_automation(False, True)
    if scenario in (2, 5):
        carrier.launch(ships[0])
    if scenario == 5:
        ids = [p(person, "person_id") for person in p(transport, "persons")]
        transport.dispatch_shuttle(ships[1], ships[2], ids[:5])
        transport.launch_pods(ships[0], ships[2])
    initial = snapshot(pc, carrier, pd, transport)
    pc.step_simulation(1)
    if snapshot(pc, carrier, pd, transport) != initial:
        raise AssertionError("Paused 2C snapshot changed")
    began = time.perf_counter()
    advance(pc, seconds, speed, batch)
    elapsed = time.perf_counter() - began
    result = snapshot(pc, carrier, pd, transport)
    report["performance"].setdefault("api_batches", []).append({
        "scenario": scenario, "ships": len(ships), "aircraft_records": len(p(carrier, "aircraft")),
        "game_seconds": seconds, "speed": speed, "batch_game_seconds": batch,
        "wall_seconds": round(elapsed, 4), "simulation_wall_ms_per_game_second": round(elapsed * 1000 / seconds, 3),
    })
    return result


def run_scenes(pc, world, carrier, pd, transport):
    combat = p(pc, "combat")
    camera = p(pc, "play_game3")
    old = p(pc, "phase2_c_scenario")
    check("2C-SCENE-REJECT", not request_scene(pc, 99) and p(pc, "phase2_c_scenario") == old)
    for scene in range(6):
        ships = start(pc, scene)
        check("2C-SCENE-" + str(scene), p(pc, "simulation_paused") and p(pc, "play_game3") == camera
              and len(ships) == 3 and p(pc, "phase2_c_scenario") == scene)
        check("2C-PD-MOUNTS-" + str(scene), all(len(p(pd.get_ship_state(s), "mounts")) ==
              sum(p(m, "type") == unreal.OVWeaponType.INTERCEPTION for m in p(p(p(s, "systems"), "layout"), "weapon_mounts")) for s in ships))
    check("2C-SHUTTLE-CLASS", p(p(p(ships[1], "systems"), "layout"), "small_hull")
          and len(p(combat.get_ship_state(ships[1]), "mounts")) == 0 and len(p(pd.get_ship_state(ships[1]), "mounts")) == 1)
    base = [battle(pc, carrier, pd, transport, 0, speed) for speed in (1, 2, 3)]
    check("2C-PD-CLOCK-123", base[0] == base[1] == base[2], base)
    check("2C-PD-ACTUAL-MISSILES", base[0]["shots"] > 0 and base[0]["pd"][0] > 0 and base[0]["pd"][1] > 0, base[0])
    air_defense = battle(pc, carrier, pd, transport, 1, 1)
    check("2C-AIRDEFENSE-LIVE", air_defense["pd"][0] > 0 and len(p(pd.get_ship_state(p(pc, "ships")[1]), "mounts")) == 11, air_defense)
    report["escort_comparison"] = {"ordinary": base[0], "air_defense": air_defense,
        "conditions": "Same seed/missile attacker; different escort hull, mount count and explicit range role. Not equal-fleet balance proof."}
    mixed = [battle(pc, carrier, pd, transport, 5, speed, seconds=18) for speed in (1, 2, 3)]
    check("2C-MIXED-CLOCK-123", mixed[0] == mixed[1] == mixed[2], mixed)
    check("2C-MIXED-LIVE", mixed[0]["aircraft_fire"][0] > 0 and mixed[0]["aircraft_fire"][1] > 0
          and mixed[0]["pd"][0] > 0 and len(p(transport, "pods")) == 5, mixed[0])
    fine = battle(pc, carrier, pd, transport, 5, 1, seconds=18, batch=1/60)
    check("2C-LOWFPS-SWEPT", fine == mixed[0], {"60Hz": fine, "250ms_game_batches": mixed[0]})
    report["same_game_time"] = {"ordinary_123": base, "mixed_123": mixed, "mixed_60Hz": fine}
    pc.start_combat_range()
    check("2C-F5-RESTORE", p(pc, "phase2_c_scenario") == -1 and not p(pd, "enabled")
          and not p(transport, "enabled") and not p(carrier, "aircraft")
          and p(p(combat, "settings"), "guaranteed_range") == 1600)
    pc.start_calibration_range(1)
    f6_ships = p(pc, "ships")
    f6_pd_counts = [sum(p(m, "type") == unreal.OVWeaponType.INTERCEPTION for m in p(p(p(s, "systems"), "layout"), "weapon_mounts")) for s in f6_ships]
    f6_pd_ok = (not any(f6_pd_counts) and not p(pd, "enabled")) or (p(pd, "enabled") and
        all(len(p(pd.get_ship_state(s), "mounts")) == count for s, count in zip(f6_ships, f6_pd_counts)))
    check("2C-F6-RESTORE", p(pc, "phase2_c_scenario") == -1 and f6_pd_ok
          and not p(transport, "enabled") and p(p(combat, "settings"), "guaranteed_range") == 25000)
    check("2C-F6-PD-REGISTRATION", f6_pd_ok, f6_pd_counts)


def run_carrier(pc, carrier, pd, transport):
    combat = p(pc, "combat")
    ships = start(pc, 2); own = ships[0]
    quiet(ships, combat, pd, carrier)
    original = craft_for(carrier, own)
    ids = [p(c, "aircraft_id") for c in original]
    hangars = [h for h in p(carrier, "hangars") if p(h, "carrier") == own]
    check("2C-HANGAR-CAPACITY", len(original) == len(hangars)*20 and len(hangars) >= 2)
    carrier.set_capacity_bonus(own, 6)
    check("2C-CAPACITY-NO-CREATION", [p(c, "aircraft_id") for c in craft_for(carrier, own)] == ids
          and all(carrier.get_hangar_capacity(p(h, "hangar_id")) == 26 for h in hangars))
    carrier.register_carrier(own, 20)
    check("2C-REGISTER-IDEMPOTENT", [p(c, "aircraft_id") for c in craft_for(carrier, own)] == ids)
    check("2C-HANGAR-EXCLUSIVE", not carrier.set_hangar_mode(p(hangars[0], "hangar_id"), unreal.OVHangarMode.SMALL_SHIP))
    pc.select_ship(own); pc.action("LaunchAircraft")
    check("2C-LAUNCH-INVENTORY", carrier.count_aircraft(own, unreal.OVAircraftState.SORTIE) == len(ids) and carrier.launch(own) == 0)
    carrier.apply_aircraft_damage(ids[0], 90)
    injured = p(craft_for(carrier, own)[0], "hp")
    pc.action("RecallAircraft"); advance(pc, 5)
    check("2C-RECALL-SAME-IDS-NO-HEAL", carrier.count_aircraft(own, unreal.OVAircraftState.STORED) == len(ids)
          and [p(c, "aircraft_id") for c in craft_for(carrier, own)] == ids and p(craft_for(carrier, own)[0], "hp") == injured)
    pc.action("LaunchAircraft")
    facility(own, unreal.OVFacilityType.FIGHTER_CONTROL, False)
    advance(pc, .25)
    check("2C-CONTROL-LOSS", carrier.count_aircraft(own, unreal.OVAircraftState.CONTROL_LOST) == len(ids))
    positions = [p(c, "position") for c in craft_for(carrier, own)]
    advance(pc, .25)
    check("2C-CONTROL-STATIONARY", all((p(c, "position")-pos).length() < .01 for c, pos in zip(craft_for(carrier, own), positions)))
    facility(own, unreal.OVFacilityType.FIGHTER_CONTROL, True)
    advance(pc, .25)
    check("2C-CONTROL-RECOVERY", carrier.count_aircraft(own, unreal.OVAircraftState.CONTROL_LOST) == 0
          and p(craft_for(carrier, own)[0], "hp") == injured)
    carrier.apply_aircraft_damage(ids[0], 100000)
    check("2C-AIRCRAFT-LOSS-ONCE", not carrier.apply_aircraft_damage(ids[0], 100000) and p(carrier, "loss_count") == 1)
    pc.action("RecallAircraft"); advance(pc, 8); pc.action("LaunchAircraft")
    check("2C-REDEPLOY-NO-RESURRECTION", carrier.count_aircraft(own, unreal.OVAircraftState.WRECKED) == 1
          and carrier.count_aircraft(own, unreal.OVAircraftState.SORTIE) == len(ids)-1)
    # Establish real old-owner movement/posture/automation orders before capture.
    own.set_destination(own.get_actor_location() + unreal.Vector(20000, 0, 2000))
    own.set_manual_attitude(unreal.Rotator(pitch=10, yaw=90, roll=5))
    own.set_automation(True, True)
    p(own, "systems").set_editor_property("faction_id", 2)
    advance(pc, .25)
    check("2C-CARRIER-CAPTURE-AFFILIATION", all(p(c, "faction_id") == 2 for c in craft_for(carrier, own) if p(c, "state") != unreal.OVAircraftState.WRECKED)
          and p(own, "hold") and not p(own, "has_destination") and not p(own, "has_manual_attitude")
          and p(own, "velocity").length() < .001 and not own.is_movement_automatic() and not own.is_attack_automatic())
    ships = start(pc, 2); own = ships[0]; quiet(ships, combat, pd, carrier)
    hangar = next(h for h in p(carrier, "hangars") if p(h, "carrier") == own)
    p(own, "systems").debug_destroy_slot(p(hangar, "slot_id"), unreal.OVDamageCause.SABOTAGE)
    advance(pc, .25)
    check("2C-HANGAR-STORED-LOSS", carrier.count_aircraft(own, unreal.OVAircraftState.WRECKED) == 20)
    ships = start(pc, 2); own = ships[0]; quiet(ships, combat, pd, carrier)
    pc.select_ship(own); pc.action("LaunchAircraft")
    facility(own, unreal.OVFacilityType.HANGAR, False)
    pc.action("TrialFTL"); advance(pc, 11)
    pc.action("TrialDeparture")
    check("2C-FTL-WAIT-RECOVERY", not combat.is_departed(own) and carrier.has_outstanding_aircraft(own))
    old_count = len(p(carrier, "aircraft"))
    pc.action("AbandonAircraft"); pc.action("TrialDeparture"); advance(pc, .25)
    check("2C-FTL-EXPLICIT-ABANDON", combat.is_departed(own) and len(p(carrier, "aircraft")) == old_count
          and carrier.count_aircraft(own, unreal.OVAircraftState.CONTROL_LOST) > 0)


def run_pods(pc, carrier, pd, transport):
    combat = p(pc, "combat")
    ships = start(pc, 3)
    pc.action("LaunchPods"); advance(pc, 7)
    casualties = transport.count_state(unreal.OVPassengerState.DEAD) + transport.count_state(unreal.OVPassengerState.RESCUE_WAITING)
    entered = transport.count_state(unreal.OVPassengerState.INSIDE_TARGET)
    check("2C-POD-LIVE-PARTIAL-PD", casualties > 0 and entered > 0 and p(pd, "kill_count") > 0,
          {"casualties": casualties, "inside": entered, "PD_kills": p(pd, "kill_count")})
    ships = start(pc, 3); own, _, target = ships
    quiet(ships, combat, pd, carrier)
    combat.set_target(own, target)
    persons = list(p(transport, "persons")); ids = [p(person, "person_id") for person in persons]
    check("2C-PERSON-IDENTITIES", len(ids) == 10 and len(set(ids)) == 10 and sum(p(x, "hero_type_dev") for x in persons) == 4)
    pc.action("LaunchPods")
    check("2C-ONE-PERSON-ONE-POD", len(p(transport, "pods")) == 10 and transport.launch_pods(own, target) == 0)
    advance(pc, 6)
    check("2C-POD-ACTUAL-ARRIVAL", transport.count_state(unreal.OVPassengerState.INSIDE_TARGET) == 10
          and len(p(transport, "entry_events")) == 10 and p(p(target, "systems"), "faction_id") == 1)
    events = p(transport, "entry_events")
    check("2C-ENTRY-LOCAL-DATA", all(1 <= p(e, "deck") <= p(p(p(target, "systems"), "layout"), "deck_count")
          and str(p(e, "slot_id")) != "None" and p(e, "local_point").length() > 0 for e in events))
    check("2C-RESIDENT-PROTECTION-NO-OBJECTIVE", combat.is_target_protected(own, target)
          and not p(target, "systems").is_interior_objective_complete(0))
    pod_ids = [p(pod, "pod_id") for pod in p(transport, "pods")]
    pc.action("DevEntryReturn"); advance(pc, 5)
    check("2C-RETURN-ORIGINAL-PODS", transport.count_state(unreal.OVPassengerState.ABOARD) == 10
          and [p(pod, "pod_id") for pod in p(transport, "pods")] == pod_ids
          and [p(person, "person_id") for person in p(transport, "persons")] == ids)
    check("2C-EXIT-PROTECTION-CLEARED", not combat.is_target_protected(own, target))
    ships = start(pc, 3); own, _, target = ships; quiet(ships, combat, pd, carrier); combat.set_target(own, target)
    p(target, "systems").reset_state()  # DEV: restore active shield, not a boarding action.
    pc.action("LaunchPods"); advance(pc, 7)
    check("2C-ACTIVE-SHIELD-BARRIER", not p(transport, "entry_events")
          and transport.count_state(unreal.OVPassengerState.RESCUE_WAITING) == 4 and transport.count_state(unreal.OVPassengerState.DEAD) == 6)
    ships = start(pc, 3); own, _, target = ships; quiet(ships, combat, pd, carrier); combat.set_target(own, target)
    pc.action("LaunchPods"); advance(pc, 6)
    p(target, "systems").debug_destroy_slot(p(p(p(target, "systems"), "layout"), "core_slot_id"), unreal.OVDamageCause.SABOTAGE)
    advance(pc, .25)
    check("2C-RESIDENT-WRECK-DEATH", transport.count_state(unreal.OVPassengerState.DEAD) == 10
          and transport.count_state(unreal.OVPassengerState.RESCUE_WAITING) == 0 and not p(target, "systems").has_interior_presence(0))
    ships = start(pc, 3); own, escort, target = ships; quiet(ships, combat, pd, carrier); combat.set_target(own, target)
    pc.action("LaunchPods"); advance(pc, .25)
    first_id = p(p(transport, "persons")[0], "person_id")
    combat.notify_ftl_departure(target); combat.notify_ftl_departure(own); advance(pc, 8)
    first = transport.get_person(first_id)
    check("2C-POD-ALTERNATE-RETURN", p(first, "state") == unreal.OVPassengerState.ABOARD and p(first, "current_ship") == escort)


def run_shuttle(pc, carrier, pd, transport):
    combat = p(pc, "combat")
    ships = start(pc, 4); quiet(ships, combat, pd, carrier)
    ids = [p(person, "person_id") for person in p(transport, "persons")]
    pc.action("DispatchShuttle"); advance(pc, 40)
    check("2C-SHUTTLE-DELIVERY", transport.count_state(unreal.OVPassengerState.INSIDE_TARGET) == 10
          and p(p(transport, "shuttles")[0], "state") == unreal.OVShuttleState.DELIVERED and not p(transport, "pods"))
    pc.action("RecallShuttle"); advance(pc, 1)
    check("2C-SHUTTLE-ENTRY-BOUNDARY", transport.count_state(unreal.OVPassengerState.INSIDE_TARGET) == 10)
    pc.action("DevEntryReturn"); advance(pc, 40)
    check("2C-SHUTTLE-SAME-RECOVERY", transport.count_state(unreal.OVPassengerState.ABOARD) == 10 and not p(transport, "pods")
          and [p(person, "person_id") for person in p(transport, "persons")] == ids)
    ships = start(pc, 4); quiet(ships, combat, pd, carrier)
    pc.action("DispatchShuttle")
    pc.set_editor_property("developer_mode", True); pc.action("DevLoseShuttle"); advance(pc, .25)
    check("2C-SHUTTLE-LOSS-BEFORE-ENTRY", transport.count_state(unreal.OVPassengerState.DEAD) == 6
          and transport.count_state(unreal.OVPassengerState.RESCUE_WAITING) == 4 and not p(transport, "pods"))
    ships = start(pc, 4); quiet(ships, combat, pd, carrier)
    pc.action("DispatchShuttle"); advance(pc, 40); pc.action("DevLoseShuttle"); advance(pc, .25)
    check("2C-SHUTTLE-LOSS-AFTER-ENTRY", transport.count_state(unreal.OVPassengerState.INSIDE_TARGET) == 10)
    pc.action("DevEntryReturn")
    rejected = not transport.recall_shuttle(ships[1])
    pc.action("RecallShuttle")
    rejection_reported = p(pc, "last_action") == p(transport, "last_message")
    advance(pc, 5)
    check("2C-NO-FREE-ESCAPE-PODS", not p(transport, "pods")
          and transport.count_state(unreal.OVPassengerState.AWAITING_RECOVERY) == 10
          and rejected and rejection_reported
          and p(p(transport, "shuttles")[0], "state") == unreal.OVShuttleState.LOST)


def run_control_regressions(pc, world, carrier, pd, transport):
    combat = p(pc, "combat")
    ships = start(pc, 0)
    pc.select_ship(ships[0])
    mounts = p(p(p(ships[0], "systems"), "layout"), "weapon_mounts")
    visited = set()
    for _ in range(len(mounts) * 2):
        pc.action("NextWeapon")
        index = p(pc, "selected_weapon")
        if p(mounts[index], "type") == unreal.OVWeaponType.INTERCEPTION:
            raise AssertionError("N selected an autonomous PD mount for offensive fire")
        visited.add(index)
    check("2C-N-CYCLE-OFFENSIVE-ONLY", len(visited) == sum(p(m, "type") != unreal.OVWeaponType.INTERCEPTION for m in mounts))
    method = getattr(pc, "start_phase2c_stress", None) or getattr(pc, "start_phase2_c_stress", None)
    if method is None:
        raise AttributeError("StartPhase2CStress API missing")
    check("2C-STRESS-ROSTER", method(1) and len(p(pc, "ships")) == 5 and len(p(carrier, "aircraft")) == 160)
    pc.action("CombatRange")
    check("2C-STRESS-F5-CLEANUP", len(p(pc, "ships")) == 3 and len(p(combat, "ships")) == 3
          and not p(pd, "ships") and not p(carrier, "aircraft")
          and len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.OVShip)) == 3)
    method(1); pc.action("CalibrationRange")
    check("2C-STRESS-F6-CLEANUP", len(p(pc, "ships")) == 3 and not p(pd, "ships") and not p(carrier, "aircraft"))
    method(1); pc.action("Phase2C")
    check("2C-STRESS-F7-CLEANUP", len(p(pc, "ships")) == 3 and len(p(pd, "ships")) == 3
          and not p(carrier, "aircraft") and len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.OVShip)) == 3)

    ships = start(pc, 4); quiet(ships, combat, pd, carrier)
    pc.action("DispatchShuttle"); advance(pc, .25)
    shuttle = ships[1]; pc.select_ship(shuttle)
    goal = shuttle.get_actor_location() + unreal.Vector(-8000, -4000, 5000)
    pc.action("Move"); pc.set_editor_property("preview", goal); pc.action("Confirm")
    advance(pc, .5)
    check("2C-SHUTTLE-MANUAL-XYZ-PRIORITY", p(p(transport, "shuttles")[0], "manual_suspended")
          and p(shuttle, "has_destination") and (p(shuttle, "destination") - goal).length() < .01)
    pc.action("Stop"); advance(pc, 2)
    stopped = shuttle.get_actor_location(); advance(pc, 1)
    check("2C-SHUTTLE-STOP-PRIORITY", p(shuttle, "hold") and not p(shuttle, "has_destination")
          and (shuttle.get_actor_location() - stopped).length() < .01)
    pc.action("RecallShuttle"); advance(pc, .25)
    check("2C-SHUTTLE-EXPLICIT-RESUME", not p(p(transport, "shuttles")[0], "manual_suspended"))

    ships = start(pc, 2); quiet(ships, combat, pd, carrier); own = ships[0]
    pc.select_ship(own); pc.action("TrialFTL"); advance(pc, 2)
    check("2C-FTL-TRIAL-CHARGING", p(own, "ftl_charging") and len(p(pc, "trial_ftl_remaining")) == 1)
    p(own, "systems").set_editor_property("faction_id", 2); advance(pc, .25)
    check("2C-FTL-CAPTURE-CLEARS-OLD-ORDER", not p(own, "ftl_charging")
          and not p(pc, "trial_ftl_remaining") and not combat.is_departed(own))


def tick(dt):
    try:
        if time.monotonic() - state["start"] > 300:
            raise TimeoutError("2C PIE validation exceeded five minutes")
        if state["phase"] == 0:
            check("2C-EDITOR-MAP", editor.get_editor_world().get_name() == "ControlRange")
            level.editor_request_begin_play(); state["phase"] = 1
            return
        if state["phase"] == 1:
            world = editor.get_game_world()
            if not world:
                return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if not pc or len(p(pc, "ships")) != 3:
                return
            check("2C-REAL-PIE", level.is_in_play_in_editor() and "UEDPIE" in world.get_path_name())
            carrier = subsystem(world, unreal.OVCarrierSubsystem)
            pd = subsystem(world, unreal.OVPointDefenseSubsystem)
            transport = subsystem(world, unreal.OVTransportSubsystem)
            state.update(pc=pc, world=world, carrier=carrier, pd=pd, transport=transport)
            run_scenes(pc, world, carrier, pd, transport)
            run_carrier(pc, carrier, pd, transport)
            run_pods(pc, carrier, pd, transport)
            run_shuttle(pc, carrier, pd, transport)
            run_control_regressions(pc, world, carrier, pd, transport)
            for _ in range(3):
                start(pc, 2); before = len(p(carrier, "aircraft")); pc.action("RestartScene")
                if len(p(carrier, "aircraft")) != before:
                    raise AssertionError("Restart duplicated aircraft inventory")
                pc.start_combat_range(); pc.start_calibration_range(1)
            check("2C-REPEATED-SCENE-RESET", not p(pd, "enabled") and not p(transport, "enabled") and not p(carrier, "aircraft"))
            start(pc, 5); pc.set_editor_property("developer_mode", False); pc.get_hud().set_details_open(False)
            state.update(phase=2, at=time.monotonic())
            return
        pc, world = state["pc"], state["world"]
        if state["phase"] == 2:
            if time.monotonic() - state["at"] < .5:
                return
            hud = pc.get_hud()
            for action in ("Pause", "LaunchAircraft", "RecallAircraft", "PDPriority"):
                center = hud.get_action_center(action)
                check("2C-HUD-ID-" + action, center.x >= 0 and center.y >= 0 and str(hud.get_hit_action_at_canvas_point(center)) == action, center)
            marker, roster = hud.get_action_center("WorldPick_0"), hud.get_action_center("Pick_0")
            width, height = pc.get_viewport_size()
            scale = max(.25, min(1, width/1280, height/740))
            pixel = unreal.Vector2D(marker.x*scale, marker.y*scale)
            check("2C-WORLD-ROSTER-DISTINCT-IDS", marker.x >= 0 and roster.x >= 0
                  and str(hud.get_hit_action_at_canvas_point(marker)) == "WorldPick_0"
                  and str(hud.get_hit_action_at_canvas_point(roster)) == "Pick_0")
            check("2C-WORLD-MARKER-CAMERA-OWNERSHIP", hud.is_over_world_marker(pixel)
                  and not hud.is_over_panel(pixel.x, pixel.y))
            hud.set_details_open(True)
            state.update(phase=3, at=time.monotonic())
            return
        if state["phase"] == 3:
            if time.monotonic() - state["at"] < .5:
                return
            hud = pc.get_hud(); position = p(hud, "window_position")
            # Window's empty left gutter must own the click even over world/HUD.
            center = unreal.Vector2D(position.x + 2, position.y + 110)
            check("2C-HUD-DETAIL-CONSUMES", str(hud.get_hit_action_at_canvas_point(center)) == "WindowBackground")
            before = snapshot(pc, state["carrier"], state["pd"], state["transport"])
            for _ in range(5):
                hud.set_details_open(False); hud.set_details_open(True)
            check("2C-HUD-REFRESH-NO-MUTATION", before == snapshot(pc, state["carrier"], state["pd"], state["transport"]))
            hud.set_details_open(False)
            for ship in p(pc, "ships"):
                ship.set_automation(False, True)
            pc.action("LaunchAircraft"); pause(pc, False)
            state.update(phase=4, at=time.monotonic(), frame_deltas=[], real_start=p(p(pc, "combat"), "game_time"))
            return
        if state["phase"] == 4:
            state["frame_deltas"].append(dt * 1000)
            if time.monotonic() - state["at"] < 5:
                return
            pause(pc, True)
            check("2C-RENDERED-FRAMES-ADVANCE", p(p(pc, "combat"), "game_time") > state["real_start"] + .1
                  and p(state["carrier"], "shot_count") > 0)
            sample = state["frame_deltas"]
            ordered = sorted(sample)
            report["performance"]["rendered_mixed_scene"] = {
                "ships": len(p(pc, "ships")), "aircraft_records": len(p(state["carrier"], "aircraft")),
                "duration_wall_s": round(time.monotonic()-state["at"], 3), "frames": len(sample),
                "mean_slate_callback_dt_ms": round(statistics.mean(sample), 3),
                "p95_slate_callback_dt_ms": round(ordered[min(len(ordered)-1, int(len(ordered)*.95))], 3),
                "max_slate_callback_dt_ms": round(max(sample), 3),
                "note": "Editor/Slate frame intervals; not isolated GPU time or packaged-game benchmark",
            }
            unreal.SystemLibrary.execute_console_command(world, "Shot SHOWUI", pc)
            start(pc, 5)
            pc.set_editor_property("developer_mode", False); pc.get_hud().set_details_open(False)
            check("2C-READY-FOR-USER", p(pc, "simulation_paused") and p(pc, "phase2_c_scenario") == 5)
            save(); unreal.unregister_slate_post_tick_callback(state["handle"])
            unreal.log("OV2C VALIDATE COMPLETE")
    except Exception:
        report["errors"].append(traceback.format_exc())
        save(); unreal.log_error(report["errors"][-1])
        if state.get("pc"):
            pause(state["pc"], True)
        unreal.unregister_slate_post_tick_callback(state["handle"])


state["handle"] = unreal.register_slate_post_tick_callback(tick)
