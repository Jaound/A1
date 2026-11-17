"""Measure how long a destructible asteroid field persists during actual fire.
This is a short terrain-removal episode, not added to full-battle win rates.
"""
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor
import hashlib
import json
import statistics
from environment_battle import EnvironmentBattle, legacy

HERE=Path(__file__).resolve().parent

def probe(task):
    hp,budget,seed=task
    b=EnvironmentBattle(legacy.roster(budget),legacy.roster(budget),seed,hazard="asteroid",
                        parameters={"hp_50m":hp},heroes=False,retreat=False,duration=600)
    destroyed_at={}
    while b.t<600:
        b.tick(.5)
        for body in b.bodies:
            if body["destroyed"] and body["id"] not in destroyed_at:
                destroyed_at[body["id"]]=b.t
        if len(destroyed_at)==len(b.bodies):break
    return {"hp_50m":hp,"budget":budget,"seed":seed,"destruction_seconds":destroyed_at,
            "last_removal_seconds":max(destroyed_at.values()) if len(destroyed_at)==len(b.bodies) else None,
            "remaining_asteroids":len(b.bodies)-len(destroyed_at),
            "blocked_shots":b.metric["env_blocked_shots"],"active_environment_count":1}

def main():
    tasks=[(hp,budget,seed) for hp in (1800.,6000.,18000.) for budget in (100,300) for seed in (175200,175201)]
    with ProcessPoolExecutor(max_workers=4) as pool:rows=list(pool.map(probe,tasks))
    summary={}
    for hp in (1800.,6000.,18000.):
        for budget in (100,300):
            chosen=[x for x in rows if x["hp_50m"]==hp and x["budget"]==budget]
            times=[x["last_removal_seconds"] for x in chosen if x["last_removal_seconds"] is not None]
            summary[f"{hp}/{budget}"]={"median_last_removal_seconds":statistics.median(times) if times else None,
                                       "censored":sum(x["last_removal_seconds"] is None for x in chosen)}
    r={"status":"completed","scope":"12 terrain-removal episodes; not full battle win-rate samples",
       "rows":rows,"summary":summary,
       "environment_sha256":hashlib.sha256((HERE/"environment_battle.py").read_bytes()).hexdigest()}
    (HERE/"asteroid_persistence_results.json").write_text(json.dumps(r,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(summary,ensure_ascii=False,indent=2))

if __name__=="__main__":main()
