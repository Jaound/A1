"""Sequential hazard batches; each battle has at most ONE environmental hazard."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import statistics
import time
from collections import Counter, defaultdict
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from environment_battle import EnvironmentBattle, DEFAULTS, legacy

HERE=Path(__file__).resolve().parent
FAMILIES={
    "asteroid": {
        "weak":{"hp_50m":800.,"collision_hp_fraction":.003,"radii":[35.,50.,65.]},
        "standard":{},
        "strong":{"hp_50m":3600.,"collision_hp_fraction":.012,"radii":[55.,80.,110.]},
    },
    "radiation": {
        "weak":{"peak_shield_fraction_per_second":.003},
        "standard":{},
        "strong":{"peak_shield_fraction_per_second":.012},
    },
    "flare": {
        "weak":{"warning":45.,"period":240.,"shield_fraction":.2,"emp_seconds":2.,"radar_seconds":4.,"radar_scale":.75},
        "standard":{},
        "strong":{"warning":60.,"period":180.,"thickness":90.,"shield_fraction":.5,"emp_seconds":5.,"radar_seconds":8.,"radar_scale":.35},
    },
    "blackhole": {
        "weak":{"peak_acceleration":.4},
        "standard":{},
        "strong":{"peak_acceleration":1.6},
    },
}

def case_definition(name):
    if name.startswith("mixed_"):
        return int(name.split("_")[1]),"mixed",False,"none"
    if name=="boarding_100":
        return 100,"mixed",True,"assault"
    if name=="battlecarrier_200":
        return 200,"xl",False,"none"
    raise ValueError(name)

def trial(task):
    start=time.monotonic()
    budget,formation,heroes,mode=case_definition(task["case"])
    b=EnvironmentBattle(legacy.roster(budget,formation),legacy.roster(budget,formation),task["seed"],
                        hazard=task["hazard"],parameters=task.get("parameters",{}),
                        avoidance=task.get("avoidance",True),
                        radiation_resistance=task.get("resistance",0),
                        mode=mode,heroes=heroes,level=10,defenders=2,
                        retreat=False,duration=task["duration"])
    r=b.run(task.get("dt",.5))
    r.update({"stage":task["stage"],"case":task["case"],"variant":task["variant"],
              "dt":task.get("dt",.5),"wall_seconds":round(time.monotonic()-start,3),
              "initial_roster":dict(Counter(legacy.roster(budget,formation))),
              "initial_signature":f'{budget}:{formation}:{int(heroes)}:{mode}:{task["seed"]}'})
    return r

def summarize(rows):
    groups=defaultdict(list)
    for r in rows:
        groups[f'{r["environment"]}/{r["variant"]}/{r["case"]}'].append(r)
    out={}
    for key,group in groups.items():
        env_hp=[sum(v for k,v in r["metrics"].items() if k.startswith("env_hull_damage_"))/r["initial_total_hull_hp"] for r in group]
        metric_names=set().union(*(r["metrics"] for r in group))
        out[key]={
            "n":len(group),"median_observed_end_seconds":statistics.median(r["seconds"] for r in group),
            "median_main_end_seconds":statistics.median([r["main_seconds"] for r in group if r["main_seconds"] is not None]) if any(r["main_seconds"] is not None for r in group) else None,
            "outcomes":dict(Counter(r["outcome"] for r in group)),
            "left_wins":sum(r["winner"]==0 for r in group),"right_wins":sum(r["winner"]==1 for r in group),
            "unresolved":sum(r["winner"] is None for r in group),
            "mean_remaining_total_cost":statistics.mean(sum(r["remaining_cost"]) for r in group),
            "mean_environment_hull_fraction":statistics.mean(env_hp),
            "mean_aircraft_surviving":statistics.mean(r["aircraft_surviving"] for r in group),
            "hero_ids_preserved":all(r["hero_id_set_unchanged"] for r in group),
            "metrics_mean":{k:statistics.mean(r["metrics"].get(k,0) for r in group) for k in sorted(metric_names)},
        }
    return out

def hashes():
    root=HERE.parents[1]
    names=["reference/SpaceEnvironmentCalibration/environment_battle.py",
           "reference/SpaceEnvironmentCalibration/run_environment_sweep.py",
           "reference/IntegratedCombat/model.py","reference/IntegratedCombat/indoor.py",
           "reference/IntegratedCombat/profile.json"]
    return {n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in names}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--stage",choices=["pilot","evaluation","sensitivity"],required=True)
    parser.add_argument("--workers",type=int,default=min(4,os.cpu_count() or 1))
    args=parser.parse_args()
    all_rows=[];tasks=[]
    if args.stage=="pilot":
        for seed in (172100,172101):
            tasks.append(dict(stage="pilot",case="mixed_100",seed=seed,hazard="none",variant="control",duration=600))
            tasks.append(dict(stage="pilot",case="mixed_100",seed=seed,hazard="none",variant="shelter_control",duration=600,
                              parameters={"static_shelter":True,"cover_radius":120.}))
        for hazard,families in FAMILIES.items():
            for variant,parameters in families.items():
                for seed in (172100,172101):
                    tasks.append(dict(stage="pilot",case="mixed_100",seed=seed,hazard=hazard,
                                      variant=variant,parameters=parameters,duration=600))
    else:
        selection=json.loads((HERE/"selected_profiles.json").read_text())
        cases=["mixed_100","mixed_200","mixed_300","boarding_100","battlecarrier_200"]
        seeds=range(173100,173104)
        if args.stage=="evaluation":
            for hazard in ("none","asteroid","radiation","flare","blackhole"):
                for case in cases:
                    for seed in seeds:
                        tasks.append(dict(stage="evaluation",case=case,seed=seed,hazard=hazard,
                                          variant="selected" if hazard!="none" else "control",
                                          parameters=selection.get(hazard,{}),duration=1800))
            for case in cases:
                for seed in seeds:
                    tasks.append(dict(stage="evaluation",case=case,seed=seed,hazard="none",
                                      variant="shelter_control",parameters={"static_shelter":True,"cover_radius":120.},duration=1800))
        else:
            for hazard in ("asteroid","flare","blackhole"):
                for seed in (174100,174101):
                    tasks.append(dict(stage="sensitivity",case="mixed_100",seed=seed,hazard=hazard,
                                      variant="unprepared",parameters=selection[hazard],avoidance=False,duration=900))
                    tasks.append(dict(stage="sensitivity",case="mixed_100",seed=seed,hazard=hazard,
                                      variant="prepared",parameters=selection[hazard],avoidance=True,duration=900))
            for resist in (0.,.25,.5):
                for seed in (174100,174101):
                    tasks.append(dict(stage="sensitivity",case="mixed_100",seed=seed,hazard="radiation",
                                      variant=f"resistance_{resist}",parameters=selection["radiation"],resistance=resist,duration=900))
            for dt in (.25,1.):
                for hazard in ("asteroid","radiation","flare","blackhole"):
                    tasks.append(dict(stage="sensitivity",case="mixed_100",seed=174200,hazard=hazard,
                                      variant=f"dt_{dt}",parameters=selection[hazard],dt=dt,duration=600))
            # Paired same-seed timestep reference, not a balance evaluation sample.
            for hazard in ("asteroid","radiation","flare","blackhole"):
                tasks.append(dict(stage="sensitivity",case="mixed_100",seed=174200,hazard=hazard,
                                  variant="dt_0.5",parameters=selection[hazard],dt=.5,duration=600))
    started=time.monotonic()
    output=HERE/f"{args.stage}_results.json"
    metadata={"stage":args.stage,"status":"running","hashes":hashes(),"tasks":len(tasks),
              "parameters":FAMILIES if args.stage=="pilot" else selection,
              "one_environment_per_battle":True,"no_ue_execution":True}
    # Hazard batches run sequentially; parallel workers only share the same hazard type.
    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        for hazard in ("none","asteroid","radiation","flare","blackhole"):
            batch=[x for x in tasks if x["hazard"]==hazard]
            if not batch:continue
            print(f"START {args.stage}: {hazard}, {len(batch)} trials",flush=True)
            futures=[pool.submit(trial,t) for t in batch]
            for future in as_completed(futures):
                r=future.result()
                assert r["active_environment_count"]<=1
                assert r["hero_id_set_unchanged"]
                all_rows.append(r)
                if len(all_rows)%4==0:
                    print(f"  finished {len(all_rows)}/{len(tasks)} ({time.monotonic()-started:.1f}s)",flush=True)
            metadata.update(rows=sorted(all_rows,key=lambda r:(r["environment"],r["variant"],r["case"],r["seed"])),
                            summary=summarize(all_rows),wall_seconds=round(time.monotonic()-started,2))
            output.write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
            print(f"END {hazard}",flush=True)
    metadata["status"]="completed"
    output.write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(f"COMPLETE {len(all_rows)} trials -> {output.name}",flush=True)

if __name__=="__main__":main()
