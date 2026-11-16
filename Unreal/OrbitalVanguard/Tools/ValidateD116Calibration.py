"""Real PIE acceptance for D116. Includes unchanged R1/2B API regression."""
import os
import unreal

tools = os.path.join(unreal.Paths.project_dir(), "Tools")
with open(os.path.join(tools, "ValidateRevisionOrders.py"), encoding="utf-8") as stream:
    exec(compile(stream.read(), "ValidateRevisionOrders.py", "exec"), globals())

def battle(pc, scenario, speed, game_seconds=32):
    assert pc.start_calibration_range(scenario)
    combat = p(pc, "combat")
    ships = p(pc, "ships")
    for ship in ships:
        if not combat.is_departed(ship):
            ship.set_automation(False, True)
    pc.action("Speed" + str(speed))
    pc.step_simulation(1)
    assert p(combat, "shot_count") == 0 and p(combat, "game_time") == 0
    pause(pc, False)
    # Deliberately drive 250 ms game-time batches through the controller. Production
    # projectile motion still sweeps every fixed 1/120 s step.
    resolved = {}
    first_wreck = None
    for frame in range(game_seconds * 4):
        pc.step_simulation(.25 / speed)
        for event in p(combat, "recent_events"):
            resolved[p(event, "attack_id")] = event
        if first_wreck is None and any(p(p(ship, "systems"), "wrecked") for ship in ships):
            first_wreck = (frame+1)*.25
    pause(pc, True)
    events = sorted(resolved.values(), key=lambda event: (p(event, "resolved_at"), p(event, "attack_id")))
    hits = [event for event in events if p(event, "result") == unreal.OVAttackResult.HIT]
    return {
        "game_seconds": round(p(combat, "game_time"), 5),
        "shots": p(combat, "shot_count"),
        "hits": p(combat, "hit_count"),
        "flying": len(p(combat, "attacks")),
        "hull": [round(p(p(ship, "systems"), "hull_hp"), 3) for ship in ships],
        "shield": [round(p(p(ship, "systems"), "shield"), 3) for ship in ships],
        "first_impact_s": round(p(hits[0], "resolved_at"), 3) if hits else None,
        "mount_hits": sum(str(p(event, "impact_weapon_id")) != "None" for event in hits),
        "armor_hits": sum(str(p(event, "armor_id")) != "None" for event in hits),
        "explosions": sum(p(event, "explosions") for event in hits),
        "mounts_lost": [sum(p(m, "hp") <= 0 for m in p(p(ship, "systems"), "weapon_mount_states")) for ship in ships],
        "wrecked": [p(p(ship, "systems"), "wrecked") for ship in ships],
        "disabled_engines": [not p(ship, "systems").can_move() for ship in ships],
        "first_wreck_sample_s": first_wreck,
        "missile_hits": sum(p(p(event, "weapon"), "kind") == unreal.OVAttackKind.MISSILE for event in hits),
        "guided_hits": sum(p(p(event, "weapon"), "kind") == unreal.OVAttackKind.GUIDED_MISSILE for event in hits),
        "cannon_hits": sum(p(p(event, "weapon"), "kind") == unreal.OVAttackKind.CANNON for event in hits),
    }

def calibration(pc, world):
    old_camera = p(pc, "play_game3")
    check("D116 rejects unknown scenario without transition", not pc.start_calibration_range(99) and p(pc, "calibration_scenario") == -1)
    check("D116 duel starts paused and preserves camera profile", pc.start_calibration_range(0) and p(pc, "simulation_paused") and p(pc, "play_game3") == old_camera)
    combat = p(pc, "combat")
    vector, corvette, bastion = p(pc, "ships")
    settings = p(combat, "settings")
    check("D116 duel inactive ship removed from combat and selection", combat.is_departed(corvette))
    check("D116 vector and bastion keep distinct layouts and loadouts",
          p(p(p(vector, "systems"), "layout"), "deck_count") == 1 and p(p(p(bastion, "systems"), "layout"), "deck_count") == 2
          and len(p(combat.get_ship_state(vector), "mounts")) == 3 and len(p(combat.get_ship_state(bastion), "mounts")) == 5)
    profiles = {str(p(w, "profile_id")): w for w in p(settings, "weapons")}
    cannon = profiles["D116_cannon_T2"]
    check("D116 normalized ranges speeds and tier damage",
          p(settings, "guaranteed_range") == 25000 and p(profiles["D116_beam_T1"], "range") == 70000
          and p(cannon, "speed") == 45000 and p(cannon, "damage") == 75
          and p(profiles["D116_spinal_T4"], "damage") == 5100 and p(profiles["D116_spinal_T4"], "speed") == 42000)
    check("D116 radar and hull candidates mapped",
          p(p(settings, "radars")[1], "range") == 105000 and p(p(vector, "systems"), "max_hull_hp") == 21252
          and p(p(vector, "systems"), "max_shield") == 6600 and p(vector, "move_speed") == 3000)
    for _ in range(2):
        assert pc.start_calibration_range(0)
    check("D116 repeated resets never multiply candidate units",
          p(p(combat, "settings"), "guaranteed_range") == 25000
          and p(p(p(combat.get_ship_state(vector), "mounts")[0], "profile"), "speed") == 45000
          and p(vector, "move_speed") == 3000)
    pc.apply_camera_input(unreal.Vector2D(), unreal.Vector(), 0, -100, 0)
    check("D116 zoom reaches expanded camera bound while paused", p(pc, "camera_distance") == 420000)
    pc.action("Focus")
    pc.apply_camera_input(unreal.Vector2D(), unreal.Vector(), 0, 0, .1)
    check("D116 follow moves focus toward selected ship", p(pc, "camera_focus").x < -10000)
    pc.action("Move")
    pc.sample_order_cursor(unreal.Vector(270000, -260000, 400000), unreal.Vector(0, 0, -1), 100, False, False)
    preview = p(pc, "preview")
    check("D116 extended projection clamps XY at calibrated bounds", preview.x == 180000 and preview.y == -180000)
    pc.sample_order_cursor(unreal.Vector(270000, -260000, 400000), unreal.Vector(0, 0, -1), 100, True, False)
    pc.sample_order_cursor(unreal.Vector(270000, -260000, 400000), unreal.Vector(0, 0, -1), -2000, True, False)
    check("D116 altitude clamp retains anchored XY", p(pc, "preview").z == 60000 and p(pc, "preview").x == 180000)
    pc.action("Confirm")
    check("D116 XYZ command reaches extended boundary", p(vector, "destination").z == 60000 and p(vector, "destination").x == 180000)
    duel = [battle(pc, 0, speed) for speed in (1, 2, 3)]
    check("D116 duel identical seed and game time across 1/2/3x low FPS batches", duel[0] == duel[1] == duel[2], duel)
    check("D116 high speed cannon sweeps hit actual duel ships", duel[0]["cannon_hits"] > 0 and duel[0]["hits"] > 0 and duel[0]["shield"] != [6600,1408,10560], duel[0])
    wing = [battle(pc, 1, speed) for speed in (1, 3)]
    check("D116 wing repeat compares only matching roster/seed/game time", wing[0] == wing[1] and wing[0]["hits"] > 0, wing)
    missiles = battle(pc, 2, 3)
    check("D116 missile fixture executes live combat", missiles["missile_hits"] > 0 and missiles["guided_hits"] > 0, missiles)
    check("D116 active missile profiles preserve real speeds and guidance",
          any(p(p(m, "profile"), "speed") == 11000 and p(p(m, "profile"), "guidance_turn_rate") == 35
              for m in p(combat.get_ship_state(bastion), "mounts")))
    report["calibration"] = {"seed": 260112, "duel": duel, "wing": wing, "missile": missiles}
    pc.start_combat_range()
    check("D116 F5 restores legacy tuning, dimensions and actor visibility",
          p(pc, "calibration_scenario") == -1 and p(p(combat, "settings"), "guaranteed_range") == 1600
          and abs(p(vector, "move_speed") - 850) < .001 and not combat.is_departed(corvette)
          and p(p(vector, "systems"), "max_shield") == 300 and p(pc, "camera_distance") == 6500)
    pc.action("Move")
    pc.sample_order_cursor(unreal.Vector(90000, 0, 50000), unreal.Vector(0, 0, -1), 0, False, False)
    check("D116 return restores legacy XY command boundary", p(pc, "preview").x == 6000)
    pc.start_calibration_range(1)

def save():
    report["passed"] = bool(report["checks"]) and all(c["passed"] for c in report["checks"]) and not report["errors"]
    report["scope"] = "D116 calibration, R1 commands and original 2B regression"
    with open(os.path.join(root, "Validation", "Phase2BRCalibration.json"), "w", encoding="utf-8") as out:
        json.dump(report, out, ensure_ascii=False, indent=2)

# Reuse the existing validators through their normal phases, then exercise calibrated
# fixtures after their last foreground hitbox assertion and before final output.
old_tick = tick
unreal.unregister_slate_post_tick_callback(state["handle"])

def tick(dt):
    if state["phase"] != 5 or state.get("calibrated"):
        return old_tick(dt)
    try:
        if time.monotonic() - state["at"] < 1:
            return
        calibration(state["pc"], state["world"])
        state["calibrated"] = True
        state["at"] = time.monotonic()
    except Exception:
        report["errors"].append(traceback.format_exc())
        save()
        unreal.log_error(report["errors"][-1])
        stop()

state["handle"] = unreal.register_slate_post_tick_callback(tick)
