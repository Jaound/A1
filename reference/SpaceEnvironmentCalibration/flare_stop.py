"""Isolated design sensitivity study; not UE or full fleet combat."""
from __future__ import annotations
import hashlib
import json
import math
import random
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROFILE_PATH = ROOT / "reference/IntegratedCombat/profile.json"
PROFILE = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
DURATIONS = (0, 2, 3, 5, 8, 12)
COUNTS = (1, 2, 3, 5, 8, 11)
VOLLEYS = (3, 6, 12)
SEEDS = tuple(range(171100, 171200))
STEP = 0.05
HORIZON = 12.0

def percentile(values, fraction):
    values = sorted(values)
    return values[max(0, math.ceil(len(values) * fraction) - 1)]

def interception(disabled, mounts, volley, seed):
    rng = random.Random(seed)
    missile = PROFILE["weapons"]["missile"]
    arrivals = sorted(rng.uniform(1.0, HORIZON) for _ in range(volley))
    hp = [float(missile["projectile_hp"])] * volley
    cooldown = [rng.uniform(0, PROFILE["global"]["pd_interval"]) for _ in range(mounts)]
    reached = 0
    for tick in range(round(HORIZON / STEP) + 1):
        time = tick * STEP
        for i, arrival in enumerate(arrivals):
            if hp[i] > 0 and time >= arrival:
                reached += 1
                hp[i] = -1
        for m in range(mounts):
            cooldown[m] = max(0.0, cooldown[m] - STEP)
            if time < disabled or cooldown[m] > 1e-9:
                continue
            # Ranges/accuracy are the documented 2C trial PD values, metres.
            pd_range, accuracy = (111.0, 0.72) if m % 2 == 0 else (126.0, 0.62)
            target = next((i for i, arrival in enumerate(arrivals)
                           if hp[i] > 0 and 0 < arrival-time <= pd_range/missile["speed"]), None)
            if target is None:
                continue
            cooldown[m] = PROFILE["global"]["pd_interval"]
            if rng.random() < accuracy * (1-missile["evasion"]):
                hp[target] -= PROFILE["global"]["pd_damage"]
    return reached

def main():
    rows = []
    for mounts in COUNTS:
        for volley in VOLLEYS:
            baseline = [interception(0, mounts, volley, s) for s in SEEDS]
            for duration in DURATIONS:
                impacts = [interception(duration, mounts, volley, s) for s in SEEDS]
                rows.append({
                    "disabled_seconds": duration, "pd_mounts": mounts, "incoming_missiles": volley,
                    "trials": len(SEEDS), "baseline_mean_reached": statistics.mean(baseline),
                    "mean_reached": statistics.mean(impacts),
                    "p95_reached": percentile(impacts, .95),
                    "mean_additional_reached": statistics.mean(impacts)-statistics.mean(baseline),
                })
    summary = []
    for duration in DURATIONS:
        selected = [r for r in rows if r["disabled_seconds"] == duration]
        summary.append({
            "disabled_seconds": duration,
            "mean_additional_reached_per_7_missile_trial": statistics.mean(r["mean_additional_reached"] for r in selected),
            "lost_firing_fraction_at_180s_interval": duration / 180,
            "lost_shield_regen_fraction_of_capacity": duration * PROFILE["global"]["shield_combat_regen"],
            "uncovered_motion_loss_m": {
                key: PROFILE["ships"][key]["speed"] * duration
                for key in ("corvette", "bastion", "carrier", "dreadnought", "assault")
            },
        })
    result = {
        "version": "flare-stop-sensitivity-v1",
        "status": "isolated design model; not full environment/fleet/hero simulation or UE certification",
        "source_profile_sha256": hashlib.sha256(PROFILE_PATH.read_bytes()).hexdigest(),
        "source_script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "assumptions": {
            "game_seconds": True, "seed_count": len(SEEDS), "seed_range": [min(SEEDS), max(SEEDS)],
            "dt": STEP, "horizon": HORIZON,
            "scope": "Unshielded exposed target, only its own PD; 3/6/12 already launched standard missiles reach it within 1..12s. No evasion after PD.",
            "cooldowns": "cooldowns progress during stop but no firing until stop ends; trial assumption",
            "omissions": [
                "flare shield damage/cover geometry/wave speed and period",
                "HP/armor damage, ship or hero survival probabilities",
                "allied PD, guided missiles, aircraft/pod flight and recovery",
                "regeneration while stopped: loss estimate assumes shield regen is suspended",
                "whole fleet tactics, moving target/obstacle geometry, actual UE timing"
            ],
            "summary_weighting": "equal weight to 18 PD-count/volley combinations; mean incoming count is 7",
            "180s_interval": "reference for fractional downtime only, not an adopted flare period"
        },
        "summary": summary,
        "rows": rows,
        "trial_count": len(COUNTS)*len(VOLLEYS)*len(DURATIONS)*len(SEEDS),
    }
    assert result["trial_count"] == 10800
    assert all(0 <= r["mean_reached"] <= r["incoming_missiles"] for r in rows)
    assert all(r["mean_additional_reached"] == 0 for r in rows if r["disabled_seconds"] == 0)
    output = Path(__file__).with_name("flare_stop_results.json")
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"trial_count": result["trial_count"], "summary": summary}, ensure_ascii=False, indent=2))

if __name__ == "__main__":
    main()
