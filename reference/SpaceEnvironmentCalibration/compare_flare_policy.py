"""Post-evaluation flare AI policy refinement; separate from main held-out samples."""
import hashlib
import json
import statistics
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from run_environment_sweep import trial, summarize

HERE=Path(__file__).resolve().parent

def main():
    profiles=json.loads((HERE/"selected_profiles.json").read_text())
    main_results=json.loads((HERE/"evaluation_results.json").read_text())
    cases=["mixed_100","mixed_200","mixed_300","boarding_100","battlecarrier_200"]
    tasks=[]
    for case in cases:
        for seed in (173100,173101):
            tasks.append(dict(stage="policy_refinement",case=case,seed=seed,hazard="flare",
                              variant="risk_based_recheck",parameters={**profiles["flare"],"cover_policy":"risk_based"},duration=1800))
    for case in ("mixed_100","boarding_100"):
        for seed in (176100,176101):
            for policy in ("always","risk_based"):
                tasks.append(dict(stage="policy_validation",case=case,seed=seed,hazard="flare",
                                  variant=policy,parameters={**profiles["flare"],"cover_policy":policy},duration=1800))
    started=time.monotonic();rows=[]
    with ProcessPoolExecutor(max_workers=4) as pool:
        futures=[pool.submit(trial,t) for t in tasks]
        for future in as_completed(futures):
            r=future.result();assert r["active_environment_count"]==1
            rows.append(r)
            print(f"flare policy {len(rows)}/{len(tasks)} ({time.monotonic()-started:.1f}s)",flush=True)
    comparisons={}
    for case in cases:
        old=[r for r in main_results["rows"] if r["environment"]=="flare" and r["case"]==case and r["seed"] in (173100,173101)]
        new=[r for r in rows if r["case"]==case and r["variant"]=="risk_based_recheck"]
        comparisons[case]={"n_each":2,"always_median_observed_end":statistics.median(r["seconds"] for r in old),
                           "risk_median_observed_end":statistics.median(r["seconds"] for r in new),
                           "always_unresolved":sum(r["winner"] is None for r in old),
                           "risk_unresolved":sum(r["winner"] is None for r in new)}
    result={"status":"completed","new_trials":len(rows),"main_reference_trials":10,
            "posthoc_recheck_trials":10,"fresh_validation_trials":8,"comparisons":comparisons,
            "summary":summarize(rows),"rows":rows,"wall_seconds":time.monotonic()-started,
            "main_reference_sha256":hashlib.sha256((HERE/"evaluation_results.json").read_bytes()).hexdigest(),
            "code_sha256":hashlib.sha256((HERE/"environment_battle.py").read_bytes()).hexdigest(),
            "caution":"Only the 8 fresh-seed trials are fresh policy validation. Do not merge rechecks into main sample or claim broad win-rate confidence."}
    (HERE/"flare_policy_results.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"comparisons":comparisons,"summary":result["summary"]},ensure_ascii=False,indent=2))

if __name__=="__main__":main()
